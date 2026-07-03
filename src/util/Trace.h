#ifndef ASHB_TRACE_H
#define ASHB_TRACE_H

// Per-entity per-tick debug tracing.
//
// The simulation used to stream hundreds of lines of std::cout per tick from
// the decision/relationship hot path. Because the Logger redirects std::cout
// into cmd_log.txt, the sim's real bottleneck was disk I/O of debug text
// (multi-hundred-MB logs per run). ASHB_TRACE_STREAM is a drop-in replacement:
// by default it swallows everything into a null stream buffer; define
// ASHB_TRACE_VERBOSE to get the old firehose back for debugging.

#include <iostream>

namespace ashb {

struct NullBuf : std::streambuf {
    int overflow(int c) override { return c; }
};

inline std::ostream& trace() {
#ifdef ASHB_TRACE_VERBOSE
    return std::cout;
#else
    static NullBuf nullBuf;
    static std::ostream nullStream(&nullBuf);
    return nullStream;
#endif
}

} // namespace ashb

#define ASHB_TRACE_STREAM ashb::trace()

#endif // ASHB_TRACE_H
