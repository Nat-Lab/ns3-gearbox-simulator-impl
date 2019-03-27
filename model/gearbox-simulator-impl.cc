#include "gearbox-simulator-impl.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GearboxSimulatorImpl");
NS_OBJECT_ENSURE_REGISTERED(GearboxSimulatorImpl);

TypeId GearboxSimulatorImpl::GetTypeId (void) {
    static TypeId tid = TypeId("ns3::GearboxSimulatorImpl")
        .SetParent<SimulatorImpl> ()
        .SetGroupName ("Core")
        .AddConstructor<GearboxSimulatorImpl> ()
        .AddAttribute("TimeFactor",
                      "Run the simulation at factor times of speed.",
                      DoubleValue (1.0),
                      MakeDoubleAccessor(&GearboxSimulatorImpl::m_factor),
                      MakeDoubleChecker<double_t> ());

    return tid;
}

GearboxSimulatorImpl::GearboxSimulatorImpl () {
    m_stop = false;
    m_running = false;
    m_nuid = 0;
    m_cuid = 0;
    m_cts = 0;
    m_ctx = Simulator::NO_CONTEXT;
    m_todo = 0;
    m_main = SystemThread::Self();
    m_synchronizer = CreateObject<GearboxSynchronizer> ();
}

GearboxSimulatorImpl::~GearboxSimulatorImpl() {}

void GearboxSimulatorImpl::DoDispose (void)
{
    while (!m_events->IsEmpty ()) {
        Scheduler::Event next = m_events->RemoveNext ();
        next.impl->Unref ();
    }
    m_events = 0;
    m_synchronizer = 0;
    SimulatorImpl::DoDispose ();
}

void GearboxSimulatorImpl::Destroy () {
    while (!m_tokill.empty()) {
        Ptr<EventImpl> ei = m_tokill.front ().PeekEventImpl ();
        if (!ei->IsCancelled()) ei->Invoke();
        m_tokill.pop_front ();
    }
}

bool GearboxSimulatorImpl::IsFinished (void) const {
    bool finished;
    {
        CriticalSection c (m_mutex);
        finished = m_events->IsEmpty() || m_stop;
    }
    return finished;
}

void GearboxSimulatorImpl::Stop (void) {
    m_stop = true;
}

void GearboxSimulatorImpl::Stop (const Time &delay) {
    Simulator::Schedule (delay, &Simulator::Stop);
}

EventId GearboxSimulatorImpl::Schedule (const Time &delay, EventImpl *ei) {
    CriticalSection c (m_mutex);

    Scheduler::Event ev;
    Time ts = Simulator::Now() + delay;
    ev.impl = ei;
    ev.key.m_ts = (uint64_t) ts.GetTimeStep ();
    ev.key.m_context = GetContext ();
    ev.key.m_uid = m_nuid++;
    ++m_todo;
    m_events->Insert (ev);
    m_synchronizer->Signal ();
    return EventId(ei, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void GearboxSimulatorImpl::ScheduleWithContext (uint32_t context, const Time &delay, EventImpl *ei) {
    CriticalSection c (m_mutex);
    uint64_t ts;

    if (SystemThread::Equals (m_main)) // schedule by script (i.e. by user)
        ts = m_cts + delay.GetTimeStep();
    else { // schedule by running thread (e.g. simulation), likely by other modules.
        ts = m_running ? m_synchronizer->GetCurrentRealtime () : m_cts;
        ts += delay.GetTimeStep();
    }

    Scheduler::Event ev;
    ev.impl = ei;
    ev.key.m_ts = ts;
    ev.key.m_context = context;
    ev.key.m_uid = m_nuid++;
    ++m_todo;
    m_events->Insert (ev);
    m_synchronizer->Signal ();
}

EventId GearboxSimulatorImpl::ScheduleNow (EventImpl *ei) {
    CriticalSection c (m_mutex);

    Scheduler::Event ev;
    ev.impl = ei;
    ev.key.m_ts = m_cts;
    ev.key.m_context = GetContext ();
    ev.key.m_uid = m_nuid++;
    ++m_todo;
    m_events->Insert (ev);
    m_synchronizer->Signal ();

    return EventId (ei, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

EventId GearboxSimulatorImpl::ScheduleDestroy (EventImpl *ei) {
    CriticalSection c (m_mutex);

    EventId id = EventId (Ptr<EventImpl> (ei, false), m_cts, 0xffffffff, 2);
    m_tokill.push_back (id);
    ++m_nuid;

    return id;
}

void GearboxSimulatorImpl::Remove (const EventId &ev) {
    if (ev.GetUid () == 2) { // 2: destroy
        for (auto i = m_tokill.begin (); i != m_tokill.end (); i++) {
            if (ev == *i) {
                m_tokill.erase(i);
                return;
            }
        }
        return;
    }

    if (IsExpired (ev)) return;

    {
        CriticalSection cs (m_mutex);

        Scheduler::Event event;
        event.impl = ev.PeekEventImpl ();
        event.key.m_ts = ev.GetTs ();
        event.key.m_context = ev.GetContext ();
        event.key.m_uid = ev.GetUid ();
        m_events->Remove (event);
        --m_todo;
        event.impl->Cancel ();
        event.impl->Unref ();
    }

    return;
}

void GearboxSimulatorImpl::Cancel (const EventId &ev) {
    if (!IsExpired(ev)) ev.PeekEventImpl ()->Cancel ();
}

bool GearboxSimulatorImpl::IsExpired (const EventId &ev) const {
    if (ev.GetUid () == 2) { // 2: destroy
        if (ev.PeekEventImpl () == 0 || ev.PeekEventImpl ()->IsCancelled())
            return true;
        for (auto i = m_tokill.begin (); i != m_tokill.end (); i++) {
            if (ev == *i) return false;
        }
        return true;
    }

    if (
        ev.PeekEventImpl () == 0 ||
        ev.GetTs() < m_cts ||
        (ev.GetTs() == m_cts && ev.GetUid () <= m_cuid) ||
        ev.PeekEventImpl ()->IsCancelled()
    ) return true;

    return false;
}

void GearboxSimulatorImpl::Run (void) {
    m_main = SystemThread::Self();
    m_stop = false;
    m_running = true;
    m_synchronizer->SetTimeFactor (m_factor);
    m_synchronizer->SetOrigin (m_cts);

    uint64_t t_now = 0;
    uint64_t t_delay = 1000; // 1ms in ns

    while (!m_stop) {
        bool todo = false;
        {
            CriticalSection cs (m_mutex);

            if (!m_events->IsEmpty()) todo = true;
            else t_now = m_synchronizer->GetCurrentRealtime ();

        }

        if (!todo) {
            m_synchronizer->Synchronize(t_now, t_delay);
            t_now = 0;
            continue;
        }

        DoNext();
    }
}

Time GearboxSimulatorImpl::Now (void) const {
    return TimeStep (m_cts);
}

Time GearboxSimulatorImpl::GetDelayLeft (const EventId &ev) const {
    if (IsExpired (ev)) return TimeStep(0);
    return TimeStep (ev.GetTs () - m_cts);
}

Time GearboxSimulatorImpl::GetMaximumSimulationTime (void) const {
    return TimeStep (0x7fffffffffffffffLL);
}

void GearboxSimulatorImpl::SetScheduler (ObjectFactory schedulerFactory) {
    CriticalSection cs (m_mutex);
    Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

    if (m_events != 0) {
        while (!m_events->IsEmpty ()) {
            scheduler->Insert(m_events->RemoveNext());
        }
    }

    m_events = scheduler;
}

uint32_t GearboxSimulatorImpl::GetSystemId (void) const {
    return 0;
}

uint32_t GearboxSimulatorImpl::GetContext (void) const {
    return m_ctx;
}


uint64_t GearboxSimulatorImpl::GetEventCount (void) const {
    return m_todo;
}

uint64_t GearboxSimulatorImpl::GetNext (void) const {
    Scheduler::Event ev = m_events->PeekNext ();
    return ev.key.m_ts;
}

void GearboxSimulatorImpl::DoNext (void) {
    while (true) {
        uint64_t next = 0;
        uint64_t delay = 0;

        uint64_t now;

        {
            CriticalSection c (m_mutex);

            now = m_synchronizer->GetCurrentRealtime ();
            next = GetNext();

            if (next < now) delay = 0;
            else delay = next - now;
                
            m_synchronizer->SetCondition (false);
        }

        if (m_synchronizer->Synchronize(now, delay)) {
            break;
        }
    }

    Scheduler::Event nev;

    {
        CriticalSection c (m_mutex);
        nev = m_events->RemoveNext ();
        --m_todo;

        m_cts = nev.key.m_ts;
        m_ctx = nev.key.m_context;
        m_cuid = nev.key.m_uid;        
    }

    EventImpl *nei = nev.impl;
    m_synchronizer->EventStart ();
    nei->Invoke ();
    m_synchronizer->EventEnd ();
    nei->Unref ();
}

void GearboxSimulatorImpl::SetTimeFactor (double_t factor) {
    m_factor = factor;
    m_synchronizer->SetTimeFactor (factor);
}

}

