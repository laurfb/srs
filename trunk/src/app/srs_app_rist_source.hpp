/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#ifndef SRS_APP_RIST_SOURCE_HPP
#define SRS_APP_RIST_SOURCE_HPP

#include <srs_core.hpp>
#include <srs_app_source.hpp>

#include <map>
#include <string>

class SrsRequest;
class SrsSharedPtrMessage;
class SrsCommonMessage;
class SrsOnMetaDataPacket;

// RIST Source - manages RIST streaming sessions and converts to RTMP
class SrsRistSource
{
private:
    static std::map<std::string, SrsRistSource*> pool_;
    
private:
    SrsRequest* req_;
    SrsLiveSource* source_;  // Underlying RTMP source
    bool is_publishing_;
    
public:
    SrsRistSource();
    virtual ~SrsRistSource();
    
public:
    virtual srs_error_t initialize(SrsRequest* req);
    
    // Publishing lifecycle
    virtual srs_error_t on_publish();
    virtual void on_unpublish();
    
    // Message handling
    virtual srs_error_t on_audio(SrsSharedPtrMessage* msg);
    virtual srs_error_t on_video(SrsSharedPtrMessage* msg);
    virtual srs_error_t on_metadata(SrsOnMetaDataPacket* metadata);
    
    // Get underlying RTMP source
    SrsLiveSource* rtmp_source() { return source_; }
    
public:
    // Static pool management
    static SrsRistSource* fetch(SrsRequest* req);
    static void store(SrsRistSource* source);
    static void dispose(SrsRequest* req);
    
private:
    static std::string generate_key(SrsRequest* req);
};

#endif // SRS_APP_RIST_SOURCE_HPP
