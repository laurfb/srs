/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#include <srs_app_rist_source.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_config.hpp>
#include <srs_protocol_amf0.hpp>
#include <srs_app_source.hpp>

std::map<std::string, SrsRistSource*> SrsRistSource::pool_;

SrsRistSource::SrsRistSource()
{
    req_ = NULL;
    source_ = NULL;
    is_publishing_ = false;
}

SrsRistSource::~SrsRistSource()
{
    srs_freep(req_);
}

srs_error_t SrsRistSource::initialize(SrsRequest* req)
{
    srs_error_t err = srs_success;
    
    req_ = req->copy();
    
    // Get or create the underlying RTMP source
    source_ = _srs_sources->fetch_or_create(req_);
    if (!source_) {
        return srs_error_new(ERROR_SYSTEM_CREATE_PIPE, "create source");
    }
    
    return err;
}

srs_error_t SrsRistSource::on_publish()
{
    srs_error_t err = srs_success;
    
    if (is_publishing_) {
        return err;
    }
    
    // Check if can publish
    if (!source_->can_publish(false)) {
        return srs_error_new(ERROR_SYSTEM_STREAM_BUSY, 
            "stream %s/%s is busy", req_->app.c_str(), req_->stream.c_str());
    }
    
    // Start publishing on RTMP source
    if ((err = source_->on_publish()) != srs_success) {
        return srs_error_wrap(err, "rtmp source publish");
    }
    
    is_publishing_ = true;
    
    srs_trace("RIST source published: %s/%s/%s",
        req_->vhost.c_str(), req_->app.c_str(), req_->stream.c_str());
    
    return err;
}

void SrsRistSource::on_unpublish()
{
    if (!is_publishing_) {
        return;
    }
    
    if (source_) {
        source_->on_unpublish();
    }
    
    is_publishing_ = false;
    
    srs_trace("RIST source unpublished: %s/%s/%s",
        req_->vhost.c_str(), req_->app.c_str(), req_->stream.c_str());
}

srs_error_t SrsRistSource::on_audio(SrsSharedPtrMessage* msg)
{
    srs_error_t err = srs_success;
    
    if (!source_) {
        return srs_error_new(ERROR_SYSTEM_STREAM_NOT_FOUND, "source not found");
    }
    
    // Convert to common message and send to RTMP source
    SrsCommonMessage* audio = msg->to_msg();
    if ((err = source_->on_audio(audio)) != srs_success) {
        srs_freep(audio);
        return srs_error_wrap(err, "consume audio");
    }
    
    return err;
}

srs_error_t SrsRistSource::on_video(SrsSharedPtrMessage* msg)
{
    srs_error_t err = srs_success;
    
    if (!source_) {
        return srs_error_new(ERROR_SYSTEM_STREAM_NOT_FOUND, "source not found");
    }
    
    // Convert to common message and send to RTMP source
    SrsCommonMessage* video = msg->to_msg();
    if ((err = source_->on_video(video)) != srs_success) {
        srs_freep(video);
        return srs_error_wrap(err, "consume video");
    }
    
    return err;
}

srs_error_t SrsRistSource::on_metadata(SrsOnMetaDataPacket* metadata)
{
    srs_error_t err = srs_success;
    
    if (!source_) {
        return srs_error_new(ERROR_SYSTEM_STREAM_NOT_FOUND, "source not found");
    }
    
    // Forward metadata to RTMP source
    if ((err = source_->on_meta_data(NULL, metadata)) != srs_success) {
        return srs_error_wrap(err, "consume metadata");
    }
    
    return err;
}

SrsRistSource* SrsRistSource::fetch(SrsRequest* req)
{
    std::string key = generate_key(req);
    
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        return it->second;
    }
    
    return NULL;
}

void SrsRistSource::store(SrsRistSource* source)
{
    if (!source || !source->req_) {
        return;
    }
    
    std::string key = generate_key(source->req_);
    pool_[key] = source;
}

void SrsRistSource::dispose(SrsRequest* req)
{
    std::string key = generate_key(req);
    
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        SrsRistSource* source = it->second;
        pool_.erase(it);
        srs_freep(source);
    }
}

std::string SrsRistSource::generate_key(SrsRequest* req)
{
    return req->vhost + "/" + req->app + "/" + req->stream;
}
