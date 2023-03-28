#include "tcp_connection.hh"
#include <sys/time.h>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/*返回当前允许的最大写入字节*/
size_t TCPConnection::remaining_outbound_capacity() const {
        return _sender.stream_in().remaining_capacity();
}

/*当前已发送但是未被确认的字节*/
size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }
/*未装配的字节*/
size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }
/*返回自从上次收到 报文段 后过去的时间*/
size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}
/*收到了一个TCPSegment*/
/*何时更新 _linger_after_streams_finish 接收端结束直接变成false*/

/*sender需要处理 ACK, ackno, win 字段 */
/*receive 需要处理 seqno, fin, syn*/
void TCPConnection::segment_received(const TCPSegment &seg) {
    /*处理fin的时候需要特别小心，直接当他装配进入后才能认为收到了fin报文段，因为可能存在 延迟到达的报文*/
    /*还需要考虑*/
    /*如果收到了一个报文，但是没有传输的内容，调用创造空报文，直接传递空报文*/
    // 如果当前还是 LISTEN 状态，那么收到的 rst 直接对其
    _time_since_last_segment_received = 0;
    if(seg.header().rst && TCPState::state_summary(_sender) != TCPSenderStateSummary::CLOSED) {
        reset();
        cerr << "Warning: Unclean shutdown of TCPConnection when in segment_received function\n";
        return;
    }

    if(!active()) {
        _tcp_state = CLOSED;
        cerr << "TCPState in CLOSED because of the rst or TIME_OUT \n";
        return;
    }
    //标记时间
    if(_tcp_state == LISTEN){
        /*处理LISTEN 状态*/
        if(!seg.header().syn) return ;
        /*携带了数据*/
        if(seg.header().syn && seg.length_in_sequence_space() != 1) return;
        _receiver.segment_received(seg);
        /*这里一定会回复一个 syn 报文 无论条件是 true 还是false*/
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV){
            fill_windows(true);
            _tcp_state = SYN_RECV;
        }
        return;
    }
    if(_tcp_state == SYN_RECV){
        if(seg.header().syn || !seg.header().ack ) return;
        _receiver.segment_received(seg);
        if(seg.header().ack) _sender.ack_received(seg.header().ackno,seg.header().win);
        fill_windows(seg.length_in_sequence_space() != 0);
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
            _tcp_state = ESTABLISHED;
        /*存在第三次握手的时候同时收到了 fin 位*/
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV)
            _tcp_state = CLOSE_WAIT;
        return;
    }
    if(_tcp_state == SYN_SENT){
        if(!seg.header().syn) return;
        _receiver.segment_received(seg);
        fill_windows(seg.length_in_sequence_space() != 0);
        /*不一定有ack，考虑极端情况 ，俩边同时建立连接 都发送了一个 syn 但是不带有ack*/
        if(seg.header().ack) _sender.ack_received(seg.header().ackno,seg.header().win);
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV)
            _tcp_state = SYN_RECV;
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
            _tcp_state = ESTABLISHED;
        return;
    }


    if(_tcp_state == ESTABLISHED){
        if(!seg.header().ack) cerr << "Illegal linked order : TCPState in ESTABLISHED but the receive seg is not a ACK seg \n";
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        fill_windows(seg.length_in_sequence_space() != 0);

        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT &&
            TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV){

            _tcp_state = CLOSING;
            _linger_after_streams_finish = false;

        } else if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT)
            _tcp_state = FIN_WAIT_1;
        else if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {

            _linger_after_streams_finish = false;
            _tcp_state = CLOSE_WAIT;

        }
        return;
    }

    if(_tcp_state == FIN_WAIT_1){

        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        fill_windows(seg.length_in_sequence_space() != 0);
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT &&
             TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {
            _tcp_state = CLOSING;
            return;
        }
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED)
            _tcp_state = FIN_WAIT_2;
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV)
            _tcp_state = TIME_WAIT;
        return;
    }

    if(_tcp_state == FIN_WAIT_2){
        /*不需要获得确认了， 因为 FIN_WAIT_2 的 sender fin 报文已经得到了确认*/
        /*_sender.ack_received(seg.header().ackno, seg.header().win);*/
        _receiver.segment_received(seg);
        fill_windows(seg.length_in_sequence_space() != 0);
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV)
            _tcp_state = TIME_WAIT;
        return;
    }
    if(_tcp_state == CLOSING){
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        /*已经收到了对方的fin, 并且已经发送了自己的fin所以只需等待确认即可， 不需要传递确认号*/
        /*fill_windows(true);*/
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED)
            _tcp_state = TIME_WAIT;
        return;
    }
    if(_tcp_state == TIME_WAIT){
        _receiver.segment_received(seg);
        /*time_wait 状态不需要 重传*/
        fill_windows(true);
        return;
    }

    if(_tcp_state == CLOSE_WAIT){
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        /*close_wait 不需要一定有回复*/
        /*通过 write 和 tick 来发送*/
        fill_windows(seg.length_in_sequence_space() != 0);
        if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT)
            _tcp_state = LAST_ACK;
        return;
    }

    if(_tcp_state == LAST_ACK) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        fill_windows(seg.length_in_sequence_space() != 0);
        if (TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED)
            _tcp_state = CLOSED;
        return;
    }
}

/*判断当前是否是活跃状态, 只有处于关闭或者 reset状态时候我们才会 设置未false*/
bool TCPConnection::active() const {
    if(_tcp_state == CLOSED || _reset
        || (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        time_since_last_segment_received() >= 10 * _cfg.rt_timeout) ){

        return false;
    }
    return true;
}
/*写入字节*/
size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    fill_windows(false);
    return size;
}
/*传入自从上次调用后的时间*/
//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if(_tcp_state == LISTEN) return;
    _time_since_last_segment_received += ms_since_last_tick;
    if(_tcp_state == TIME_WAIT && _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _tcp_state = CLOSED;
    }
    _sender.tick(ms_since_last_tick);
    /*还需要判断最大重传次数， 过多那么设置reset*/
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_reset_segment();
        return;
    }
    fill_windows(false);

}
/*结束输入状态*/
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    /*存在一种情况，窗口已经满了，fin实际上是不能马上传出去的*/

    if(_tcp_state == ESTABLISHED || _tcp_state == CLOSE_WAIT) fill_windows(false);
    //主动关闭
    if(_tcp_state == ESTABLISHED &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT){
        _tcp_state = FIN_WAIT_1;
        return;
    }
    //被动关闭
    if(_tcp_state == CLOSE_WAIT &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT){
        _tcp_state = LAST_ACK;
    }
}
/*连接*/
void TCPConnection::connect() {
    if(_tcp_state != LISTEN) return;
    _sender.fill_window();
    /*发送syn同步报文*/
    if(_sender.segments_out().size() != 1){
        std::cerr << "Impossible position, fin segment create tow or more segment\n";
        return ;
    }
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
    _tcp_state = SYN_SENT;
}

/*析构函数，结束连接*/
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection when delete TCPConnection\n";
            send_reset_segment();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::reset(){
    _reset = true;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}
void TCPConnection::send_reset_segment(){
    TCPSegment tcp_segment{};
    //送达 rst报文段
    tcp_segment.header().rst = true;
    _segments_out.push(tcp_segment);
    reset();
}

void TCPConnection::fill_windows(bool must_reply) {
    _sender.fill_window();
    size_t size = _sender.segments_out().size();
    while (!_sender.segments_out().empty()) {
        TCPSegment tcpSegment = _sender.segments_out().front();
        _sender.segments_out().pop();
        // 添加 ackno 和 ack 和 win
        if(_receiver.ackno().has_value()){
            tcpSegment.header().ackno = _receiver.ackno().value();
            tcpSegment.header().ack = true;
            tcpSegment.header().win = _receiver.window_size();
        }
        // 直接 送出
        _segments_out.push(tcpSegment);
    }
    if(!must_reply) return;
    if (size != 0) return;
    //当前没有报文 返回一个 空报文
    _sender.send_empty_segment();
    TCPSegment tcpSegment = _sender.segments_out().front();
    _sender.segments_out().pop();
    tcpSegment.header().ackno = _receiver.ackno().value();
    tcpSegment.header().ack = true;
    tcpSegment.header().win = _receiver.window_size();
    // 直接 送出
    _segments_out.push(tcpSegment);
}

