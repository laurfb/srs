/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#include <srs_app_rist_server.hpp>
#include <srs_app_rist_listener.hpp>
#include <srs_app_rist_conn.hpp>
#include <srs_app_config.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_utility.hpp>

SrsRistServerConf::SrsRistServerConf()
{
    enabled = false;
    port = 1971;
    listen = "0.0.0.0";
    maxbw = 15000000;
    buffer = 200000;
    latency = 120;
    connect_timeout = 3000;
    peer_idle_timeout = 10000;
    pkt_drop = true;
    default_app = "live";
}

SrsRistServerConf::~SrsRistServerConf()
{
}

srs_error_t SrsRistServerConf::parse(SrsConfDirective* conf)
{
    srs_error_t err = srs_success;
    
    enabled = srs_config_bool(conf, "enabled", true);
    
    if (!enabled) {
        return err;
    }
    
    std::string listen_str = srs_config_string(conf, "listen", "1971");
    
    // Parse listen address (format: [ip:]port)
    size_t pos = listen_str.find(':');
    if (pos != std::string::npos) {
        listen = listen_str.substr(0, pos);
        port = atoi(listen_str.substr(pos + 1).c_str());
    } else {
        port = atoi(listen_str.c_str());
    }
    
    if (port <= 0 || port > 65535) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, 
            "invalid RIST port: %d", port);
    }
    
    maxbw = srs_config_int(conf, "maxbw", 15000000);
    buffer = srs_config_int(conf, "buffer", 200000);
    latency = srs_config_int(conf, "latency", 120);
    connect_timeout = srs_config_int(conf, "connect_timeout", 3000);
    peer_idle_timeout = srs_config_int(conf, "peer_idle_timeout", 10000);
    pkt_drop = srs_config_bool(conf, "pkt_drop", true);
    default_app = srs_config_string(conf, "default_app", "live");
    
    return err;
}

SrsRistServer::SrsRistServer()
{
    conn_manager_ = new SrsResourceManager("rist", true);
}

SrsRistServer::~SrsRistServer()
{
    if (conn_manager_) {
        srs_freep(conn_manager_);
    }
    
    for (std::vector<SrsRistListener*>::iterator it = listeners_.begin(); 
         it != listeners_.end(); ++it) {
        SrsRistListener* listener = *it;
        srs_freep(listener);
    }
    listeners_.clear();
}

srs_error_t SrsRistServer::initialize()
{
    srs_error_t err = srs_success;
    return err;
}

srs_error_t SrsRistServer::listen()
{
    srs_error_t err = srs_success;
    
    if ((err = listen_rist()) != srs_success) {
        return srs_error_wrap(err, "listen RIST");
    }
    
    return err;
}

srs_error_t SrsRistServer::cycle()
{
    srs_error_t err = srs_success;
    
    // Accept connections from all listeners
    while (true) {
        for (std::vector<SrsRistListener*>::iterator it = listeners_.begin();
             it != listeners_.end(); ++it) {
            SrsRistListener* listener = *it;
            
            if ((err = accept_rist(listener)) != srs_success) {
                srs_error("RIST: accept connection failed");
                srs_freep(err);
            }
        }
        
        // Check for cycle termination
        if (_srs_config->is_full_state()) {
            return srs_success;
        }
        
        // Sleep briefly to avoid busy loop
        st_usleep(10 * 1000);  // 10ms
    }
    
    return err;
}

void SrsRistServer::remove(ISrsResource* c)
{
    conn_manager_->remove(c);
}

srs_error_t SrsRistServer::listen_rist()
{
    srs_error_t err = srs_success;
    
    SrsConfDirective* conf = _srs_config->get_rist_server();
    if (!conf) {
        return err;  // RIST server not configured
    }
    
    SrsRistServerConf rist_conf;
    if ((err = rist_conf.parse(conf)) != srs_success) {
        return srs_error_wrap(err, "parse RIST config");
    }
    
    if (!rist_conf.enabled) {
        srs_trace("RIST server is disabled");
        return err;
    }
    
    // Create listener
    SrsRistListener* listener = new SrsRistListener(this, rist_conf);
    listeners_.push_back(listener);
    
    if ((err = listener->listen()) != srs_success) {
        return srs_error_wrap(err, "RIST listener");
    }
    
    srs_trace("RIST server listening on %s:%d", 
        rist_conf.listen.c_str(), rist_conf.port);
    
    return err;
}

srs_error_t SrsRistServer::accept_rist(SrsRistListener* listener)
{
    srs_error_t err = srs_success;
    
    // Accept new connection
    int fd = -1;
    std::string peer_ip;
    int peer_port = 0;
    
    if ((err = listener->accept(&fd, peer_ip, peer_port)) != srs_success) {
        if (srs_error_code(err) == ERROR_SOCKET_TIMEOUT) {
            srs_freep(err);
            return srs_success;  // Timeout is not an error
        }
        return srs_error_wrap(err, "accept");
    }
    
    srs_trace("RIST: accept connection from %s:%d", peer_ip.c_str(), peer_port);
    
    // Create connection handler
    SrsRistConn* conn = new SrsRistConn(this, fd, peer_ip, peer_port);
    conn_manager_->add(conn);
    
    if ((err = conn->start()) != srs_success) {
        return srs_error_wrap(err, "start connection");
    }
    
    return err;
}
