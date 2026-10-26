#pragma once

#include <cstddef>
#include <cstdint>

// A streaming NMEA 0183 parser for the sentences the clock needs: RMC (time,
// date, fix, position), GGA (satellites used) and GSV (satellites in view).
namespace rc {

struct GpsState {
  bool fix = false;         // the latest RMC has status 'A'
  int satsUsed = 0;         // GGA field 7
  int satsInView = 0;       // GSV field 3, summed over talkers (GP, GL, GA, BD, GB, GQ …)
  bool utcValid = false;    // an 'A' RMC with both time and date has been seen
  int64_t utcEpochMs = 0;   // that RMC's UTC
  int64_t utcAtMs = -1;     // monotonic ms when it was parsed
  double lat = 0, lon = 0;  // degrees, south and west negative; valid while fix
  int64_t lastFixMs = -1;   // monotonic ms of the latest 'A' RMC
};

class NmeaParser {
 public:
  void feed(const uint8_t* data, size_t n, int64_t nowMs);
  const GpsState& state() const { return state_; }
  void reset();  // the GPS was powered off: back to GpsState{}

 private:
  static constexpr size_t kMaxSentence = 82;  // '$' through "\r\n"
  static constexpr size_t kMaxTalkers = 8;

  struct TalkerCount {
    char id[2];
    int count;
  };

  void handleLine(int64_t nowMs);
  void handleRmc(char** f, int nf, int64_t nowMs);
  void handleGga(char** f, int nf);
  void handleGsv(const char* talker, char** f, int nf);

  GpsState state_;
  char buf_[96];
  size_t len_ = 0;
  bool inSentence_ = false;
  TalkerCount talkers_[kMaxTalkers] = {};
  size_t nTalkers_ = 0;
};

}  // namespace rc
