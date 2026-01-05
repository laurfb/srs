/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#ifndef SRS_APP_RIST_SERVER_HPP
#define SRS_APP_RIST_SERVER_HPP

#include <srs_core.hpp>
#include <srs_app_listener.hpp>
#include <srs_app_st.hpp>

#include <string>
#include <vector>

class SrsRistListener;
class SrsRistConn;
class SrsConfDirective;

// RIST Server Configuration
class SrsRistServerConf
{
public:
    bool enabled;
    int port;
    std::string listen;
    int maxbw;
    int buffer;
    int latency;
    int connect_timeout;
    int peer_idle_timeout;
    bool pkt_drop;
    std::string default_app;
    
public:
    SrsRistServerConf();
    virtual ~SrsRistServerConf();
    
public:
    srs_error_t parse(SrsConfDirective* conf);
};

// RIST Server
class SrsRistServer : public ISrsResourceManager
{
private:
    std::vector<SrsRistListener*> listeners_;
    SrsResourceManager* conn_manager_;
    
public:
    SrsRistServer();
    virtual ~SrsRistServer();
    
public:
    virtual srs_error_t initialize();
    virtual srs_error_t listen();
    virtual srs_error_t cycle();
    
public:
    // ISrsResourceManager
    virtual void remove(ISrsResource* c);
    
private:
    srs_error_t listen_rist();
    srs_error_t accept_rist(SrsRistListener* listener);
};

#endif // SRS_APP_RIST_SERVER_HPP
