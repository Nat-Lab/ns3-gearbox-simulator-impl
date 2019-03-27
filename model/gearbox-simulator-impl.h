#ifndef GEARBOX_SIMULATOR_H
#define GEARBOX_SIMULATOR_H

#include "gearbox-synchronizer.h"

#include <vector>
#include <algorithm>

#include "ns3/simulator.h"
#include "ns3/simulator-impl.h"
#include "ns3/system-thread.h"
#include "ns3/scheduler.h"
#include "ns3/synchronizer.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/system-mutex.h"
#include "ns3/double.h"

namespace ns3 {

class GearboxSimulatorImpl : public SimulatorImpl {
public:
    static TypeId GetTypeId (void);

    GearboxSimulatorImpl();
    ~GearboxSimulatorImpl();

    // from SimulatorImpl
    virtual void Destroy ();
    virtual bool IsFinished (void) const;
    virtual void Stop (void);
    virtual void Stop (const Time &delay);
    virtual EventId Schedule (const Time &delay, EventImpl *ei);
    virtual void ScheduleWithContext (uint32_t context, const Time &delay, EventImpl *ei);
    virtual EventId ScheduleNow (EventImpl *ei);
    virtual EventId ScheduleDestroy (EventImpl *ei);
    virtual void Remove (const EventId &ev);
    virtual void Cancel (const EventId &ev);
    virtual bool IsExpired (const EventId &ev) const;
    virtual void Run (void);
    virtual Time Now (void) const;
    virtual Time GetDelayLeft (const EventId &ev) const;
    virtual Time GetMaximumSimulationTime (void) const;
    virtual void SetScheduler (ObjectFactory schedulerFactory);
    virtual uint32_t GetSystemId (void) const;
    virtual uint32_t GetContext (void) const;
    virtual uint64_t GetEventCount (void) const; 

    // to set time factor
    void SetTimeFactor(double_t factor);

private:
    uint64_t GetNext (void) const;
    void DoNext (void);
    virtual void DoDispose (void);

    bool m_stop;
    bool m_running;
    double_t m_factor;
    uint32_t m_nuid;
    uint32_t m_cuid;
    uint32_t m_todo;
    uint64_t m_cts;
    uint32_t m_ctx;

    mutable SystemMutex m_mutex; 
    Ptr<Scheduler> m_events;
    Ptr<GearboxSynchronizer> m_synchronizer;
    SystemThread::ThreadId m_main;

    std::list<EventId> m_tokill;
};

}

#endif /* GEARBOX_SIMULATOR_H */

