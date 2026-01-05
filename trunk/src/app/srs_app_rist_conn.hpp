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

#ifndef SRS_APP_RIST_CONN_HPP
#define SRS_APP_RIST_CONN_HPP

#include <srs_core.hpp>
#include <srs_app_st.hpp>
#include <srs_app_conn.hpp>
#include <srs_protocol_rist.hpp>

#include <string>

class SrsRistServer;
class SrsRequest;
class SrsSharedPtrMessage;
class SrsCommonMessage;
class SrsOnMetaDataPacket;
class SrsRistSource;
class SrsFormat;

class SrsRistConn : public ISrsStartableConneciton, public ISrsResource
{
private:
    SrsRistServer* server_;
    SrsRistSocket* rist_socket_;
    SrsCoroutine* trd_;
    int fd_;
    std::string peer_ip_;
    int peer_port_;
    SrsRequest* req_;
    SrsRistSource* source_;
    SrsFormat* format_;
    bool is_client_;
    
public:
    SrsRistConn(SrsRistServer* server, int fd, std::string peer_ip, int peer_port);
    virtual ~SrsRistConn();
    
public:
    // ISrsResource
    virtual const SrsContextId& get_id();
    virtual std::string desc();
    
public:
    // ISrsStartableConneciton
    virtual srs_error_t start();
    
public:
    virtual srs_error_t cycle();
    
private:
    virtual srs_error_t do_cycle();
    virtual srs_error_t publishing();
    virtual srs_error_t process_publish_message(uint8_t* data, size_t size);
    virtual srs_error_t parse_stream_id(std::string stream_id);
};

#endif // SRS_APP_RIST_CONN_HPP
