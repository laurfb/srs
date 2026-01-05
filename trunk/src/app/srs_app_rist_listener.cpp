/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

#include <srs_app_rist_listener.hpp>
#include <srs_app_rist_server.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

SrsRistListener::SrsRistListener(SrsRistServer* server, const SrsRistServerConf& conf)
{
    server_ = server;
    conf_ = conf;
    rist_socket_ = NULL;
    fd_ = -1;
}

SrsRistListener::~SrsRistListener()
{
    if (rist_socket_) {
        rist_socket_->stop();
        srs_freep(rist_socket_);
    }
    
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

srs_error_t SrsRistListener::listen()
{
    srs_error_t err = srs_success;
    
    // Build endpoint URL
    char url[256];
    snprintf(url, sizeof(url), "rist://%s:%d?mode=listener", 
        conf_.listen.c_str(), conf_.port);
    ep_server_ = url;
    
    // Create and initialize RIST socket
    rist_socket_ = new SrsRistSocket();
    
    if ((err = rist_socket_->initialize(false, true)) != srs_success) {
        return srs_error_wrap(err, "initialize RIST socket");
    }
    
    if ((err = rist_socket_->add_peer(ep_server_)) != srs_success) {
        return srs_error_wrap(err, "add RIST peer: %s", ep_server_.c_str());
    }
    
    if ((err = rist_socket_->start()) != srs_success) {
        return srs_error_wrap(err, "start RIST listener");
    }
    
    fd_ = rist_socket_->get_fd();
    
    srs_trace("RIST listener started on %s", ep_server_.c_str());
    
    return err;
}

srs_error_t SrsRistListener::accept(int* pfd, std::string& peer_ip, int& peer_port)
{
    srs_error_t err = srs_success;
    
    // For RIST, connections are handled differently than traditional sockets
    // We check if there's incoming data which indicates a new peer connection
    
    // This is a simplified approach - in a production system, you'd need
    // to implement proper peer management using RIST callbacks
    
    // For now, return the RIST socket fd
    if (fd_ >= 0) {
        *pfd = fd_;
        peer_ip = "0.0.0.0";  // Will be updated by connection handler
        peer_port = 0;
        return err;
    }
    
    return srs_error_new(ERROR_SOCKET_ACCEPT, "RIST listener not ready");
}

srs_error_t SrsRistListener::create_socket()
{
    srs_error_t err = srs_success;
    
    // Socket creation is handled by librist
    
    return err;
}
