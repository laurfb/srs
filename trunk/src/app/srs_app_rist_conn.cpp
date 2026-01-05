/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#include <srs_app_rist_conn.hpp>
#include <srs_app_rist_server.hpp>
#include <srs_app_rist_source.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_config.hpp>
#include <srs_protocol_utility.hpp>
#include <srs_protocol_amf0.hpp>
#include <srs_app_source.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_protocol_format.hpp>

SrsRistConn::SrsRistConn(SrsRistServer* server, int fd, std::string peer_ip, int peer_port)
{
    server_ = server;
    fd_ = fd;
    peer_ip_ = peer_ip;
    peer_port_ = peer_port;
    
    rist_socket_ = new SrsRistSocket();
    rist_socket_->set_fd(fd);
    
    trd_ = new SrsSTCoroutine("rist", this);
    req_ = new SrsRequest();
    source_ = NULL;
    format_ = NULL;
    is_client_ = false;
}

SrsRistConn::~SrsRistConn()
{
    srs_freep(trd_);
    srs_freep(req_);
    
    if (rist_socket_) {
        rist_socket_->stop();
        srs_freep(rist_socket_);
    }
    
    srs_freep(format_);
}

const SrsContextId& SrsRistConn::get_id()
{
    return trd_->cid();
}

std::string SrsRistConn::desc()
{
    return "RistConn";
}

srs_error_t SrsRistConn::start()
{
    srs_error_t err = srs_success;
    
    if ((err = trd_->start()) != srs_success) {
        return srs_error_wrap(err, "coroutine");
    }
    
    return err;
}

srs_error_t SrsRistConn::cycle()
{
    srs_error_t err = do_cycle();
    
    // Cleanup
    if (source_) {
        source_->on_unpublish();
        source_ = NULL;
    }
    
    // Remove from connection manager
    server_->remove(this);
    
    return err;
}

srs_error_t SrsRistConn::do_cycle()
{
    srs_error_t err = srs_success;
    
    srs_trace("RIST connection from %s:%d started", peer_ip_.c_str(), peer_port_);
    
    // Read first packet to determine stream ID and mode
    uint8_t buf[2048];
    size_t size = sizeof(buf);
    
    if ((err = rist_socket_->recvmsg(buf, &size, SRS_UTIME_NO_TIMEOUT)) != srs_success) {
        return srs_error_wrap(err, "recv first packet");
    }
    
    // Parse stream ID from RIST extension or use default
    std::string stream_id = "";  // Extract from RIST metadata
    
    if ((err = parse_stream_id(stream_id)) != srs_success) {
        return srs_error_wrap(err, "parse stream ID");
    }
    
    srs_trace("RIST stream: vhost=%s, app=%s, stream=%s",
        req_->vhost.c_str(), req_->app.c_str(), req_->stream.c_str());
    
    // Start publishing cycle
    if ((err = publishing()) != srs_success) {
        return srs_error_wrap(err, "publishing");
    }
    
    return err;
}

srs_error_t SrsRistConn::publishing()
{
    srs_error_t err = srs_success;
    
    // Get or create source
    SrsRequest* req = req_->copy();
    
    source_ = SrsRistSource::fetch(req);
    if (!source_) {
        source_ = new SrsRistSource();
        if ((err = source_->initialize(req)) != srs_success) {
            return srs_error_wrap(err, "init source");
        }
        SrsRistSource::store(source_);
    }
    
    // Notify source of publishing
    if ((err = source_->on_publish()) != srs_success) {
        return srs_error_wrap(err, "on publish");
    }
    
    // Create format reader for TS demuxing
    format_ = new SrsFormat();
    
    // Read and process packets
    while (true) {
        uint8_t buf[2048];
        size_t size = sizeof(buf);
        
        if ((err = rist_socket_->recvmsg(buf, &size, 30 * SRS_UTIME_SECONDS)) != srs_success) {
            if (srs_error_code(err) == ERROR_SOCKET_TIMEOUT) {
                srs_trace("RIST connection timeout");
                srs_freep(err);
                break;
            }
            return srs_error_wrap(err, "recv packet");
        }
        
        if ((err = process_publish_message(buf, size)) != srs_success) {
            return srs_error_wrap(err, "process message");
        }
    }
    
    return err;
}

srs_error_t SrsRistConn::process_publish_message(uint8_t* data, size_t size)
{
    srs_error_t err = srs_success;
    
    // Demux TS packet and convert to RTMP messages
    SrsBuffer stream;
    if ((err = stream.initialize((char*)data, size)) != srs_success) {
        return srs_error_wrap(err, "init buffer");
    }
    
    // Use format to demux TS
    if ((err = format_->on_ts_message(&stream)) != srs_success) {
        return srs_error_wrap(err, "demux TS");
    }
    
    // Get audio/video messages and send to source
    SrsSharedPtrMessage* msg = NULL;
    while ((msg = format_->audio()) != NULL) {
        if ((err = source_->on_audio(msg)) != srs_success) {
            srs_freep(msg);
            return srs_error_wrap(err, "consume audio");
        }
        srs_freep(msg);
    }
    
    while ((msg = format_->video()) != NULL) {
        if ((err = source_->on_video(msg)) != srs_success) {
            srs_freep(msg);
            return srs_error_wrap(err, "consume video");
        }
        srs_freep(msg);
    }
    
    return err;
}

srs_error_t SrsRistConn::parse_stream_id(std::string stream_id)
{
    srs_error_t err = srs_success;
    
    // Parse stream ID (similar to SRT streamid format)
    // Format: #!::r=vhost/app/stream,m=publish
    
    if (stream_id.empty()) {
        // Use default from config
        SrsConfDirective* conf = _srs_config->get_rist_server();
        if (conf) {
            SrsRistServerConf rist_conf;
            rist_conf.parse(conf);
            
            req_->vhost = RTMP_VHOST_DEFAULT;
            req_->app = rist_conf.default_app;
            req_->stream = "livestream";
        } else {
            req_->vhost = RTMP_VHOST_DEFAULT;
            req_->app = "live";
            req_->stream = "livestream";
        }
        return err;
    }
    
    // Simple parser for stream ID
    // TODO: Implement full YAML-style parsing
    size_t r_pos = stream_id.find("r=");
    if (r_pos != std::string::npos) {
        size_t end_pos = stream_id.find(',', r_pos);
        std::string resource = stream_id.substr(r_pos + 2, 
            end_pos != std::string::npos ? end_pos - r_pos - 2 : std::string::npos);
        
        // Parse vhost/app/stream
        std::vector<std::string> parts;
        srs_string_split(resource, "/", parts);
        
        if (parts.size() >= 2) {
            req_->app = parts[0];
            req_->stream = parts[1];
        }
        
        req_->vhost = RTMP_VHOST_DEFAULT;
    }
    
    return err;
}
