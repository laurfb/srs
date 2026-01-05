/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#ifndef SRS_APP_RIST_LISTENER_HPP
#define SRS_APP_RIST_LISTENER_HPP

#include <srs_core.hpp>
#include <srs_protocol_rist.hpp>
#include <string>

class SrsRistServer;
class SrsRistServerConf;

class SrsRistListener
{
private:
    SrsRistServer* server_;
    SrsRistSocket* rist_socket_;
    SrsRistServerConf conf_;
    std::string ep_server_;
    int fd_;
    
public:
    SrsRistListener(SrsRistServer* server, const SrsRistServerConf& conf);
    virtual ~SrsRistListener();
    
public:
    virtual srs_error_t listen();
    virtual srs_error_t accept(int* pfd, std::string& peer_ip, int& peer_port);
    
private:
    srs_error_t create_socket();
};

#endif // SRS_APP_RIST_LISTENER_HPP
