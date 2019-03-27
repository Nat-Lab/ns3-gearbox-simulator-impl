#ifndef GEARBOX_SYNC_H
#define GEARBOX_SYNC_H

#include "ns3/system-condition.h"
#include "ns3/synchronizer.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <ctime>
#include <sys/time.h>

namespace ns3 {

class GearboxSynchronizer : public Synchronizer {
public:
    static TypeId GetTypeId (void);

    GearboxSynchronizer();
    ~GearboxSynchronizer();

    void SetTimeFactor (double_t factor);

    static const uint64_t US_PER_NS = (uint64_t) 1000;
    static const uint64_t US_PER_SEC = (uint64_t) 1000000;
    static const uint64_t NS_PER_SEC = (uint64_t) 1000000000;

protected:
    // from Synchronizer
    virtual void DoSetOrigin (uint64_t ns);
    virtual bool DoRealtime (void);
    virtual uint64_t DoGetCurrentRealtime (void);
    virtual bool DoSynchronize (uint64_t now, uint64_t delay);
    virtual void DoSignal (void);
    virtual void DoSetCondition (bool cond);
    virtual int64_t DoGetDrift (uint64_t ns);
    virtual void DoEventStart (void);
    virtual uint64_t DoEventEnd (void);

    // ideas from WallClockSynchronizer
    bool SpinWaitTo (uint64_t ns);
    bool SleepWait (uint64_t ns);

    uint64_t GetTime (void);
    uint64_t GetNormalizedTime (void);
    uint64_t TimevalToNs (struct timeval *tv);
    
    uint64_t m_jiffy;
    uint64_t m_sinit;
    uint64_t m_einit;

    SystemCondition m_sc;

private:
    double_t m_factor;
    uint64_t m_offset;
    bool DoSynchronizePriv (uint64_t now, uint64_t delay);
};

}

#endif // GEARBOX_SYNC_H