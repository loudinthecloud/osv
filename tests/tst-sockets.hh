#ifndef TST_SOCKETS_H
#define TST_SOCKETS_H

#include "sched.hh"

extern "C" {
    #include <bsd/sys/sys/socket.h>
    #include <bsd/sys/netinet/in.h>
    #include <bsd/include/arpa/inet.h>
}

#include <osv/poll.h>

#include "debug.hh"
#include "tst-hub.hh"

#define dbg_d(...)   logger::instance()->wrt("tst-sockets", logger_error, __VA_ARGS__)

const int stup_maxfds = 5;
const int stup_fport = 5000;
class socket_test_udp_poll {

public:
    socket_test_udp_poll() {}
    virtual ~socket_test_udp_poll() {}

    int poller(void)
    {
        /* Create 5 UDP Socket */
        for (int i=0; i < stup_maxfds; ++i) {
            struct sockaddr_in si_me;

            if ((fds[i]=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
                dbg_d("poller() socket() failed %d", fds[i]);
                return -1;
            }

            memset((char *) &si_me, 0, sizeof(si_me));
            si_me.sin_family = AF_INET;
            si_me.sin_port = htons(stup_fport + i);
            si_me.sin_addr.s_addr = htonl(INADDR_ANY);
            si_me.sin_len = sizeof(struct sockaddr_in);
            if (bind(fds[i], (const sockaddr*)&si_me, sizeof(si_me)) == -1) {
                dbg_d("poller() bind() failed %d", fds[i]);
                return -1;
            }
        }

        /* Do 5 Polls */
        for (int p=0; p < stup_maxfds; ++p) {

            /* Init request */
            struct pollfd pfds[stup_maxfds] = {};
            for (int i=0; i<5; i++) {
                pfds[i].fd = fds[i];
                pfds[i].events = POLLRDNORM;
                pfds[i].revents = 0;
            }

            dbg_d("Calling poll()");
            int error = poll(pfds, stup_maxfds, 10000);
            dbg_d("poll() result=%d completed:", error);

            /* Check result */
            for (int i=0; i<5; i++) {
                if (pfds[i].revents & POLLRDNORM) {
                    char buf[512];
                    struct sockaddr_in si_other;
                    socklen_t slen = sizeof(si_other);

                    dbg_d("    FD=%d revents=%d", pfds[i].fd, pfds[i].revents);

                    if (recvfrom(pfds[i].fd, buf, 512, 0, (sockaddr*)&si_other, &slen)==-1) {
                        dbg_d("    recvfrom failed");
                        return -1;
                    }

                    dbg_d("    %s:%d says: \"%s\"",
                        inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
                }
            }
        }

        /* Close file descriptors */
        for (int i=0; i<5; i++) {
            close(fds[i]);
        }

        return 0;
    }

    /* Connect each time to a different fd and send data on it */
    int connector(void)
    {
        for (int i=0; i<5; i++) {
            struct sockaddr_in si_other;
            int s, slen=sizeof(si_other);
            const char *message = "this is a poll() test message";

            if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
                dbg_d("connector() socket() failed %d", s);
                return -1;
            }

            memset((char *) &si_other, 0, sizeof(si_other));
            si_other.sin_family = AF_INET;
            si_other.sin_port = htons(stup_fport+i);
            si_other.sin_len = sizeof(struct sockaddr_in);

            if (inet_aton("127.0.0.1", &si_other.sin_addr)==0) {
                dbg_d("connector() inet_aton() failed %d", s);
                return -1;
            }

            dbg_d("Sending packet %d on socket %d", i, s);

            if (sendto(s, message, strlen(message)+1, 0,
                (const struct sockaddr*)&si_other, slen)==-1) {
                dbg_d("connector() sendto() failed %d", s);
                return -1;
            }

            close(s);
            sleep(1);
        }

        return 0;
    }

    int run(void)
    {
        dbg_d("POLL Test - Begin");
        memset(fds, 0, sizeof(fds));

        sched::detached_thread* t1 = new sched::detached_thread([&] {
            poller();
        });

        sched::detached_thread* t2 = new sched::detached_thread([&] {
            connector();
        });

        t1->start();
        sleep(1);
        t2->start();

        sleep(10);
        dbg_d("POLL Test - End");
        return 0;
    }

private:
    int fds[stup_maxfds];
};

#define UT_BUFLEN 512
#define UT_NPACK 5
#define UT_PORT 9930

/*
 * Based on code found here http://www.abc.se/~m6695/udp.html
 */
class socket_test_simple_udp {
public:
    socket_test_simple_udp() {}
    virtual ~socket_test_simple_udp() {}

    int udp_server(void)
    {
        struct sockaddr_in si_me, si_other;
        int s, i;
        socklen_t slen=sizeof(si_other);
        char buf[UT_BUFLEN];

        /* Create UDP Socket */
        if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
            dbg_d("udp_server() socket() failed %d", s);
            return -1;
        }

        memset((char *) &si_me, 0, sizeof(si_me));
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(UT_PORT);
        si_me.sin_addr.s_addr = htonl(INADDR_ANY);
        si_me.sin_len = sizeof(struct sockaddr_in);
        if (bind(s, (const sockaddr*)&si_me, sizeof(si_me))==-1) {
            dbg_d("udp_server() bind() failed %d", s);
            return -1;
        }

        for (i=0; i<UT_NPACK; i++) {
            if (recvfrom(s, buf, UT_BUFLEN, 0, (sockaddr*)&si_other, &slen)==-1) {
                dbg_d("recvfrom failed");
                return -1;
            }

            dbg_d("%s:%d says: \"%s\"",
                inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
        }

        close(s);

        dbg_d("udp_server() finished!");
        return 0;
    }

    int udp_client(void)
    {
        struct sockaddr_in si_other;
        int s, i, slen=sizeof(si_other);
        const char *messages[UT_NPACK] = {
            "Irish handcuffs: When a person is carrying beer in both hands.",
            "glasshole: A person who constantly talks to their Google Glass.",
            "please advise: Corporate Jargon for What The Fuck.",
            "trash jenga: When the garbage gets piled so high...",
            "going commandtoe: when you wear shoes without socks"
        };

        if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
            dbg_d("udp_client() socket() failed %d", s);
            return -1;
        }

        memset((char *) &si_other, 0, sizeof(si_other));
        si_other.sin_family = AF_INET;
        si_other.sin_port = htons(UT_PORT);
        si_other.sin_len = sizeof(struct sockaddr_in);

        if (inet_aton("127.0.0.1", &si_other.sin_addr)==0) {
            dbg_d("udp_client() inet_aton() failed %d", s);
            return -1;
        }

        for (i=0; i<UT_NPACK; i++) {
            dbg_d("Sending packet %d", i);

            if (sendto(s, messages[i], strlen(messages[i])+1, 0,
                (const struct sockaddr*)&si_other, slen)==-1) {
                dbg_d("udp_client() sendto() failed %d", s);
                return -1;
            }
        }

        close(s);
        return 0;
    }

    void run(void)
    {
        sched::detached_thread* t1 = new sched::detached_thread([&] {
            udp_server();
        });

        sched::detached_thread* t2 = new sched::detached_thread([&] {
            udp_client();
        });

        t1->start();
        t2->start();
        sleep(1);
    }

private:
};

class socket_test_openclose {
public:
    socket_test_openclose() {}
    virtual ~socket_test_openclose() {}

    int run(void)
    {
        int error, s;

        dbg_d("OC: Creating a socket...");

        s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        dbg_d("OC:     result=%d", s);
        if (s < 0) {
            dbg_d("OC:     socket() failed!");
            return -1;
        }

        dbg_d("OC: Closing socket %d", s);
        error = close(s);
        if (error < 0) {
            dbg_d("OC:     close() failed!");
            return -1;
        }


        return 0;
    }

private:

};

//
// Main tst-hub class
//
class test_sockets : public unit_tests::vtest {
public:
    test_sockets() {}
    virtual ~test_sockets() {}

    virtual void run(void)
    {
        dbg_d("Sockets Test - Begin");

#if 0
        /* Open - Close test */
        socket_test_openclose oc;
        oc.run();

        /* Simple UDP test */
        socket_test_simple_udp su;
        su.run();
#endif

        /* Poll test */
        socket_test_udp_poll up;
        up.run();

        dbg_d("Sockets Test - End");
    }
};

#undef UT_BUFLEN
#undef UT_NPACK
#undef UT_PORT

#undef dbg_d

#endif
