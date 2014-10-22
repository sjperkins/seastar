/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#include "tcp.hh"
#include "core/align.hh"

namespace net {

void tcp_option::parse(tcp_hdr* th) {
    auto hdr = reinterpret_cast<uint8_t*>(th);
    auto beg = hdr + sizeof(tcp_hdr);
    auto end = hdr + th->data_offset * 4;
    while (beg < end) {
        auto kind = option_kind(*beg);
        if (kind != option_kind::nop && kind != option_kind::eol) {
            // Make sure there is enough room for this option
            auto len = *(beg + 1);
            if (beg + len > end) {
                return;
            }
        }
        switch (kind) {
        case option_kind::mss:
            _mss_received = true;
            _remote_mss = reinterpret_cast<mss*>(beg)->mss;
            ntoh(_remote_mss);
            beg += option_len::mss;
            break;
        case option_kind::win_scale:
            _win_scale_received = true;
            _remote_win_scale = reinterpret_cast<win_scale*>(beg)->shift;
            // We can turn on win_scale option, 7 is Linux's default win scale size
            _local_win_scale = 7;
            beg += option_len::win_scale;
            break;
        case option_kind::sack:
            _sack_received = true;
            beg += option_len::sack;
            break;
        case option_kind::nop:
            beg += option_len::nop;
            break;
        case option_kind::eol:
            return;
        default:
            // Ignore options we do not understand
            auto len = *(beg + 1);
            beg += len;
            // Prevent infinite loop
            if (len == 0) {
                return;
            }
            break;
        }
    }
}

uint8_t tcp_option::fill(tcp_hdr* th, uint8_t options_size) {
    auto hdr = reinterpret_cast<uint8_t*>(th);
    auto off = hdr + sizeof(tcp_hdr);
    uint8_t size = 0;

    if (th->f_syn) {
        if (_mss_received || !th->f_ack) {
            auto mss = new (off) tcp_option::mss;
            mss->mss = _local_mss;
            off += mss->len;
            size += mss->len;
            hton(*mss);
        }
        if (_win_scale_received || !th->f_ack) {
            auto win_scale = new (off) tcp_option::win_scale;
            win_scale->shift = _local_win_scale;
            off += win_scale->len;
            size += win_scale->len;
        }
    }
    if (size > 0) {
        // Insert NOP option
        auto size_max = align_up(uint8_t(size + 1), tcp_option::align);
        while (size < size_max - uint8_t(option_len::eol)) {
            new (off) tcp_option::nop;
            off += option_len::nop;
            size += option_len::nop;
        }
        new (off) tcp_option::eol;
        size += option_len::eol;
    }
    assert(size == options_size);

    return size;
}

uint8_t tcp_option::get_size() {
    uint8_t size = 0;
    if (_mss_received)
        size += option_len::mss;
    if (_win_scale_received)
        size += option_len::win_scale;
    size += option_len::eol;
    // Insert NOP option to align on 32-bit
    size = align_up(size, tcp_option::align);
    return size;
}

}
