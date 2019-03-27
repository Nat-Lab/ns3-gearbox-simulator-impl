#include "gearbox-synchronizer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("GearboxSynchronizer");
NS_OBJECT_ENSURE_REGISTERED(GearboxSynchronizer);

TypeId GearboxSynchronizer::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::GearboxSynchronizer")
        .SetParent<Synchronizer> ()
        .SetGroupName ("Core")
        .AddConstructor<GearboxSynchronizer> ()
        .AddAttribute("TimeFactor",
                      "Run the simulation at factor times of speed.",
                      DoubleValue (1.0),
                      MakeDoubleAccessor(&GearboxSynchronizer::m_factor),
                      MakeDoubleChecker<double_t> ());

    return tid;
}

GearboxSynchronizer::GearboxSynchronizer () {
    m_offset = 0;
    m_factor = 1;
#ifdef CLOCK_REALTIME
    struct timespec ts;
    clock_getres (CLOCK_REALTIME, &ts);
    m_jiffy = ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
#else
    m_jiffy = 1000000;
#endif
}

GearboxSynchronizer::~GearboxSynchronizer() {}

void GearboxSynchronizer::SetTimeFactor (double_t factor) {
    m_factor = factor;
}

void GearboxSynchronizer::DoSetOrigin (uint64_t ns) {
    m_sinit = GetTime ();
}

bool GearboxSynchronizer::DoRealtime (void) {
    return false;
}

uint64_t GearboxSynchronizer::DoGetCurrentRealtime (void) {
    // we are not realtime, tho.
    return GetNormalizedTime();
}

bool GearboxSynchronizer::DoSynchronizePriv (uint64_t now, uint64_t delay) {
    uint64_t n_jiffy = delay / m_jiffy;

    if (n_jiffy > 3) {
        if (!SleepWait ((n_jiffy - 3) * m_jiffy)) {
            return false;
        }
    }

    uint64_t drift = DoGetDrift(now + delay);

    if (drift >= 0) return true;
    return SpinWaitTo (now + delay);
}

bool GearboxSynchronizer::DoSynchronize (uint64_t now, uint64_t delay) {
    uint64_t f_delay = delay / m_factor;
    bool ret = DoSynchronizePriv (now, f_delay);
    m_offset += delay - f_delay;
    return ret;
}

void GearboxSynchronizer::DoSignal (void) {
    m_sc.SetCondition (true);
    m_sc.Signal ();
}

void GearboxSynchronizer::DoSetCondition (bool cond) {
    m_sc.SetCondition (cond);
}

int64_t GearboxSynchronizer::DoGetDrift (uint64_t ns) {
    uint64_t now = GetNormalizedTime ();
    if (now > ns) return (int64_t) (now - ns);
    return ~(int64_t) (now - ns); 
}

void GearboxSynchronizer::DoEventStart (void) {
    m_einit = GetNormalizedTime ();
}

uint64_t GearboxSynchronizer::DoEventEnd (void) {
    return GetNormalizedTime() - m_einit;
}

uint64_t GearboxSynchronizer::GetTime (void) {
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return TimevalToNs (&tv) + m_offset;
}

uint64_t GearboxSynchronizer::GetNormalizedTime (void) {
    return GetTime () - m_sinit;
}

uint64_t GearboxSynchronizer::TimevalToNs (struct timeval *tv) {
    return tv->tv_sec * NS_PER_SEC + tv->tv_usec * US_PER_NS;
}

bool GearboxSynchronizer::SpinWaitTo (uint64_t ns) {
    while (true) {
        if (GetNormalizedTime () >= ns) return true;
        if (m_sc.GetCondition ()) return false;
    }

    return true; // UNREACHED
}

bool GearboxSynchronizer::SleepWait (uint64_t ns) {
    return m_sc.TimedWait (ns);
}
 

}