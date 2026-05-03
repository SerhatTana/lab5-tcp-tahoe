/*
 * tcp_sim.c -- TCP Tahoe Congestion Control Simulator
 *
 * CSE 320 Lab 5
 * Student 1: 20220808001 Serhat Buğra Tana
 * Student 2: 20220808006 Alperen Ulukaya
 *
 * Algorithm: TCP Tahoe  (mod = 0)
 *
 * Build : gcc -o tcp_sim tcp_sim.c
 * Run   : ./tcp_sim tahoe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                            */
/* ------------------------------------------------------------------ */
#define MSS          1          /* 1 MSS used as base unit             */
#define INIT_CWND    1          /* Slow Start initial window           */
#define INIT_SSTHRESH 64        /* Initial slow-start threshold        */
#define MAX_RTT      50         /* Simulation length (number of RTTs)  */

/* ------------------------------------------------------------------ */
/*  Tahoe state                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    double cwnd;       /* Congestion window (in MSS)                   */
    double ssthresh;   /* Slow-start threshold                         */
    int    dup_acks;   /* Consecutive duplicate ACK counter            */
} TahoeState;

/* ------------------------------------------------------------------ */
/*  Helper: print current state                                          */
/* ------------------------------------------------------------------ */
static void print_state(int rtt, const TahoeState *s, const char *event)
{
    printf("RTT %2d | cwnd=%6.1f | ssthresh=%6.1f | %s\n",
           rtt, s->cwnd, s->ssthresh, event);
}






/* ------------------------------------------------------------------ */
/*  Tahoe -- single RTT step                                             */
/* ------------------------------------------------------------------ */
/*
 * In a real network cwnd is updated on every ACK; here we simulate
 * one RTT as a single step:
 *   - Slow Start   : cwnd < ssthresh  -> cwnd doubles each RTT
 *                    (each ACK: +1 MSS => cwnd ACKs per RTT)
 *   - Cong. Avoid. : cwnd >= ssthresh -> cwnd increases by +1 MSS per RTT
 *
 * Loss scenarios are injected externally (inside simulate_tahoe).
 */
static void tahoe_on_ack(TahoeState *s)
{
    if (s->cwnd < s->ssthresh) {
        /* Slow Start: exponential increase */
        s->cwnd *= 2;
        if (s->cwnd > s->ssthresh)
            s->cwnd = s->ssthresh;  /* do not exceed ssthresh */
    } else {
        /* Congestion Avoidance: linear increase */
        s->cwnd += MSS;
    }
    s->dup_acks = 0;
}

/* Timeout or 3 dup-ACKs -> in Tahoe both trigger the same reaction */
static void tahoe_on_loss(TahoeState *s)
{
    s->ssthresh = s->cwnd / 2.0;
    if (s->ssthresh < 1) s->ssthresh = 1;
    s->cwnd     = MSS;          /* Return to Slow Start */
    s->dup_acks = 0;
}

/* ------------------------------------------------------------------ */
/*  Main simulation -- Tahoe                                             */
/* ------------------------------------------------------------------ */
static void simulate_tahoe(void)
{
    TahoeState s;
    s.cwnd     = INIT_CWND;
    s.ssthresh = INIT_SSTHRESH;
    s.dup_acks = 0;

    printf("=================================================\n");
    printf("        TCP Tahoe -- Congestion Control Sim      \n");
    printf("=================================================\n");
    printf("%-6s | %-8s | %-10s | %s\n",
           "RTT", "cwnd", "ssthresh", "Event");
    printf("-------------------------------------------------\n");

    print_state(0, &s, "Initial (cwnd=1 MSS)");

    for (int rtt = 1; rtt <= MAX_RTT; rtt++) {

        /* --- Loss injection ------------------------------------------ */

        /* Scenario 1: Timeout at RTT 8 */
        if (rtt == 8) {
            tahoe_on_loss(&s);
            print_state(rtt, &s, "TIMEOUT -> ssthresh=cwnd/2, cwnd=1, Slow Start");
            continue;
        }

        /* Scenario 2: Triple Duplicate ACK at RTT 18 */
        if (rtt == 18) {
            tahoe_on_loss(&s);
            print_state(rtt, &s, "3 DUP ACK -> ssthresh=cwnd/2, cwnd=1, Slow Start (Tahoe)");
            continue;
        }

        /* Scenario 3: Timeout at RTT 35 */
        if (rtt == 35) {
            tahoe_on_loss(&s);
            print_state(rtt, &s, "TIMEOUT -> ssthresh=cwnd/2, cwnd=1, Slow Start");
            continue;
        }

        /* --- Normal ACK ---------------------------------------------- */
        const char *phase = (s.cwnd < s.ssthresh) ? "Slow Start" : "Cong. Avoid.";
        tahoe_on_ack(&s);
        print_state(rtt, &s, phase);
    }

    printf("=================================================\n\n");
}

/* ------------------------------------------------------------------ */
/*  main                                                                  */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    /* Run simulation if no argument or argument is "tahoe" */
    if (argc < 2 || strcmp(argv[1], "tahoe") == 0) {
        simulate_tahoe();
    } else {
        fprintf(stderr, "Usage: %s tahoe\n", argv[0]);
        fprintf(stderr, "  This simulation implements TCP Tahoe only.\n");
        return 1;
    }

    return 0;
}
