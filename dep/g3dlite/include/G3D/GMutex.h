/** 
  @file GMutex.h
   
  @created 2005-09-22
  @edited  2009-03-25
 */

#ifndef G3D_GMutex_h
#define G3D_GMutex_h

#include "G3D/platform.h"
#include "G3D/AtomicInt32.h"
#include "G3D/debugAssert.h"
#include <string>

#ifndef G3D_WIN32
#   include <pthread.h>
#   include <signal.h>
#   include <ctime>
#   include <cerrno>
#endif


namespace G3D {

/**
   \brief A mutual exclusion lock that busy-waits when locking.

   On a machine with one (significant) thread per processor core,
   a spinlock may be substantially faster than a mutex.

   \sa G3D::GThread, G3D::GMutex, G3D::AtomicInt32
 */
class Spinlock {
private:

    AtomicInt32   x;

public:

    inline Spinlock() : x(0) {}

    /** Busy waits until the lock is unlocked, then locks it
        exclusively.  Returns true if the lock succeeded on the first
        try (indicating no contention). */
    inline bool lock() {
        bool first = true;
        while (x.compareAndSet(0, 1) == 1) {
            first = false;
#           ifdef G3D_WIN32
                Sleep(0);
#           else
                my_sleep(0);
#           endif
        }
        return first;
    }

    inline void unlock() {
        x.compareAndSet(1, 0);
    }

    void my_sleep(unsigned msec) {
        struct timespec req, rem;
        int err;
        req.tv_sec = msec / 1000;
        req.tv_nsec = (msec % 1000) * 1000000;
        while ((req.tv_sec != 0) || (req.tv_nsec != 0)) {
            if (nanosleep(&req, &rem) == 0)
                break;
            err = errno;
            // Interrupted; continue
            if (err == EINTR) {
                req.tv_sec = rem.tv_sec;
                req.tv_nsec = rem.tv_nsec;
            }
            // Unhandleable error (EFAULT (bad pointer), EINVAL (bad timeval in tv_nsec), or ENOSYS (function not supported))
            break;
        }
    }

};

/**
 \brief Mutual exclusion lock used for synchronization.

 @sa G3D::GThread, G3D::AtomicInt32, G3D::Spinlock
*/
class GMutex {
private:
#   ifdef G3D_WIN32
    CRITICAL_SECTION                    m_handle;
#   else
    pthread_mutex_t                     m_handle;
    pthread_mutexattr_t                 m_attr;
#   endif

    // Not implemented on purpose, don't use
    GMutex(const GMutex &mlock);
    GMutex &operator=(const GMutex &);
    bool operator==(const GMutex&);

public:
    GMutex();
    ~GMutex();

    /** Locks the mutex or blocks until available. */
    void lock();

    /** Locks the mutex if it not already locked.
        Returns true if lock successful, false otherwise. */
    bool tryLock();

    /** Unlocks the mutex. */
    void unlock();
};


/**
    Automatically locks while in scope.
*/
class GMutexLock {
private:
    GMutex* m;

    // Not implemented on purpose, don't use
    GMutexLock(const GMutexLock &mlock);
    GMutexLock &operator=(const GMutexLock &);
    bool operator==(const GMutexLock&);

public:
    GMutexLock(GMutex* mutex) {
        m = mutex;
        m->lock();
    }

    ~GMutexLock() {
        m->unlock();
    }
};

} // G3D

#endif
