#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <bitset>
#include <cstring>
#include <sys/time.h>

const uint16_t MSS = 1024; // MAX IS 1032 but header is 8 bytes
const uint16_t HEADER_LEN = 8; // bytes
const uint16_t INITIAL_SSTHRESH = 3000; // bytes
const uint16_t INITIAL_TIMEOUT = 1000; // ms, 1 sec since RTO adaption
const uint16_t MSN = 30720; // bytes

class Packet
{
public:
    Packet() = default;
    Packet(bool syn, bool ack, bool fin, uint16_t seq_num, uint16_t ack_num, uint16_t recv_window, const char *data, size_t len)
    {
        m_syn = syn;
        m_ack = ack;
        m_fin = fin;
        m_seq_num = seq_num;
        m_ack_num = ack_num;
        m_recv_window = recv_window;
        strncpy(m_data, data, len);
    }

    bool syn_set() const
    {
        return m_syn;
    }

    bool ack_set() const
    {
        return m_ack;
    }

    bool fin_set() const
    {
        return m_fin;
    }

    uint16_t seq_num() const
    {
        return m_seq_num;
    }

    uint16_t ack_num() const
    {
        return m_ack_num;
    }

    uint16_t recv_window() const
    {
        return m_recv_window;
    }

    void data(char* buffer, size_t len) const
    {
        strncpy(buffer, m_data, len);
    }

private:
    bool     m_syn:1;
    bool     m_ack:1;
    bool     m_fin:1;
    uint16_t m_seq_num;  // 2 bytes
    uint16_t m_ack_num;  // 2 bytes
    uint16_t m_recv_window; // 2 bytes
    char     m_data[MSS]; // MSS bytes
};

class Packet_info
{
public:
    Packet_info() = default;
    Packet_info(Packet p, uint16_t data_len, const struct timeval &timeout)
    {
        m_p = p;
        m_data_len = data_len;
        update_time(timeout);
    }

    Packet pkt() const
    {
        return m_p;
    }

    uint16_t data_len() const
    {
        return m_data_len;
    }

    struct timeval get_max_time() const
    {
        return m_max_time;
    }

    struct timeval get_time_sent() const
    {
        return m_time_sent;
    }

    void update_time(const struct timeval &timeout)
    {
        gettimeofday(&m_time_sent, NULL);

        // find max_time for first packet
        timeradd(&m_time_sent, &timeout, &m_max_time);
    }

private:
    Packet m_p;
    struct timeval m_time_sent;
    struct timeval m_max_time;
    uint16_t       m_data_len;
};

class RTO
{
public:
    RTO()
    {
        m_timeout.tv_usec = INITIAL_TIMEOUT * 1000; // microseconds
        timerclear(&m_EstimatedRTT);
        timerclear(&m_DevRTT);
    }

    struct timeval get_timeout() const
    {
        return m_timeout;
    }

    void update_RTO(const struct timeval &time_sent)
    {
        struct timeval curr_time;
        gettimeofday(&curr_time, NULL);

        struct timeval SampleRTT;
        timersub(&curr_time, &time_sent, &SampleRTT);

        multiply_timeval(SampleRTT, .125);
        multiply_timeval(m_EstimatedRTT, .175);

        timeradd(&m_EstimatedRTT, &SampleRTT, &m_EstimatedRTT);

        struct timeval time_diff;
        if (timercmp(&m_EstimatedRTT, &SampleRTT, >))
        {
            timersub(&m_EstimatedRTT, &SampleRTT, &time_diff);
        }
        else
        {
            timersub(&SampleRTT, &m_EstimatedRTT, &time_diff);
        }

        multiply_timeval(time_diff, .25);
        multiply_timeval(m_DevRTT, .75);

        timeradd(&m_DevRTT, &time_diff, &m_DevRTT);

        struct timeval new_DevRTT;
        new_DevRTT.tv_sec = m_DevRTT.tv_sec;
        new_DevRTT.tv_usec = m_DevRTT.tv_usec;

        multiply_timeval(new_DevRTT, 4);
        timeradd(&m_EstimatedRTT, &new_DevRTT, &m_timeout);
        m_timeout.tv_sec = 0;
        m_timeout.tv_usec = 0;
    }

    void double_RTO()
    {
        multiply_timeval(m_timeout, 2);
    }

    void multiply_timeval(struct timeval &timeval, double factor)
    {
        timeval.tv_sec *= factor;
        timeval.tv_usec *= factor;
    }

private:
    struct timeval m_EstimatedRTT;
    struct timeval m_DevRTT;
    struct timeval m_timeout;
};
#endif
