#include <GameEngine/consts.h>
#include <Raphael/SEE.h>
#include <Raphael/tests.h>


using std::cout;
using std::flush;
using std::lock_guard;
using std::mutex;
using std::runtime_error;
using std::vector;
namespace ch = std::chrono;



namespace raphael {
namespace test {
i32 test_see() {
    const i32 P = SEE::internal::VAL[0];
    const i32 N = SEE::internal::VAL[1];
    const i32 B = SEE::internal::VAL[2];
    const i32 R = SEE::internal::VAL[3];
    const i32 Q = SEE::internal::VAL[4];

    struct TestData {
        const char* fen;
        const char* mv;
        i32 exchange;
    };

    // modified version from https://github.com/zzzzz151/Starzix/blob/main/tests/SEE.txt
    static const vector<TestData> see_testdata = {
        {"6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - -",               "f1f4",  P - R + B                        },
        {"5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - -",               "d6f4",  -B + N                           },
        {"4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -",                 "h5g4",  0                                },
        {"4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -",               "h5g4",  0                                },
        {"4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -",         "g4f3",  -B + N                           },
        {"2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -",   "d6e5",  P                                },
        {"7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -",                "e1e8",  0                                },
        {"6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -",                 "e1e8",  -R                               },
        {"7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -",               "e1e8",  -R                               },
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -",                                "f7f8q", B - P                            },
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -",                                "f7f8n", N - P                            },
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -",                                  "f7f8q", Q - P                            },
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -",                                  "f7f8b", B - P                            },
        {"7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -",                                 "f7f8r", -P                               },
        {"8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -",                   "h1f1",  0                                },
        {"8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -",                   "h1f1",  0                                },
        {"2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -",                   "c5c1",  R - Q + R                        },
        {"r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -",      "f3e5",  P                                },
        {"6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -",             "f4e5",  0                                },
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6",            "g5h6",  0                                },
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6",          "g5h6",  P                                },
        {"2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -",               "c3c8",  R                                },
        {"2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -",                   "c3c5",  B                                },
        {"8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -",                        "g2c6",  P - B                            },
        {"4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -",            "e6e4",  P - R                            },
        {"4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -",            "h7e4",  P                                },
        {"3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - -",                        "d3d4",  P - R + N - P + N - B + R - Q + R},
        {"1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",               "d3e5",  N - N + B - R + N                },
        {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",              "d3e5",  P - N                            },
        {"rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q -",           "h4f6",  N - B + P                        },
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - -",  "g4f3",  N - B                            },
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq -",    "c6d4",  P - N + N - P                    },
        {"r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ -",         "e6g7",  B - N                            },
        {"r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ -",           "e6g7",  B                                },
        {"rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -",    "d6h2",  P - B                            },
        {"rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -",    "d6h2",  P                                },
        {"r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq -", "g4f3",  N - B + P                        },
        {"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ -",      "b4c3",  N - B                            },
        {"4rrk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -",        "b6b2",  P - Q                            },
        {"4rrk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -",       "f6d5",  P - N                            },
        {"1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -",              "b8b3",  P - R                            },
        {"1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - -",                 "b8b4",  P                                },
        {"2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - -",            "c8c1",  R - R                            },
        {"5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - -",                    "f5c2",  P - B + R - Q + R                },
        {"1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - -",             "a5a2",  P - Q                            },
        {"1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - -",            "a5a2",  P                                },
        {"r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - -", "d5e7",  B - N                            },
        {"rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - -",        "d3e4",  P                                },
        {"rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -",   "e4d6",  -N + P                           },
        {"r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -",   "e4d6",  -N + N                           },
        {"2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k -",          "d5c6",  -B                               },
        {"2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k -",         "d5c6",  -B + B                           },
        {"2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - -",         "d5c6",  -B + B - N                       },
        {"5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -",                "d7d8q", Q - P                            },
        {"4rk2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -",               "d7d8q", Q - P - Q                        },
        {"5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ -",                "d7d8q", Q - P - Q + B                    },
        {"4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk -",              "c7c8q", Q - P - Q                        },
        {"4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -",              "c7c8q", Q - P - Q + B                    },
        {"4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6",          "g5f6",  P - P                            },
        {"4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6",          "g5f6",  P - P                            },
        {"4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6",           "g5f6",  P - P                            },
        {"1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk -",          "c7b8q", N + Q - P - Q                    },
        {"rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq -",     "g1h2",  B                                },
        {"3N4/2K5/2n5/1k6/8/8/8/8 b - -",                                     "c6d8",  N - N                            },
        {"3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -",                                  "c7d8q", (N + Q - P) - Q + R              },
        {"r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - -",                               "e6d8",  N                                },
        {"8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - -",                                 "e3d1",  N - N                            },
        {"8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - -",                                 "c3d1",  N - (N + Q - P) + Q              },
        {"r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq -",   "e1g1",  0                                },
    };

    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "test_see: starting\n" << flush;
    }

    i32 failed = 0;
    for (const auto& testdata : see_testdata) {
        const chess::Board board(testdata.fen);
        const auto move = chess::uci::uciToMove(board, testdata.mv);

        const bool res1 = SEE::see(move, board, testdata.exchange - 1);
        const bool res2 = SEE::see(move, board, testdata.exchange);
        const bool res3 = SEE::see(move, board, testdata.exchange + 1);
        if (!(res1 && res2 && !res3)) {
            lock_guard<mutex> lock(cout_mutex);
            cout << "test_see: FAIL: " << testdata.fen << " " << testdata.mv
                 << ": expected 1 1 0, got " << res1 << " " << res2 << " " << res3 << "\n"
                 << flush;
            failed++;
        }
    }

    if (failed == 0) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "test_see: all passed\n" << flush;
    } else {
        lock_guard<mutex> lock(cout_mutex);
        cout << "test_see: failed " << failed << " test cases\n" << flush;
    }

    return failed;
}

i32 run_all(bool raise) {
    i32 failed = 0;
    failed += test_see();

    if (raise && failed > 0) throw runtime_error("1 or more test cases failed");
    return failed;
}
}  // namespace test



namespace bench {
void run(Raphael& engine) {
    // from https://github.com/Ciekce/Stormphrax/blob/main/src/bench.cpp
    static const vector<const char*> bench_data = {
        "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq - 0 14",
        "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
        "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
        "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
        "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
        "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
        "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
        "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
        "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
        "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
        "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
        "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
        "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq - 0 17",
        "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
        "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
        "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
        "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
        "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
        "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
        "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
        "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
        "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
        "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
        "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
        "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
        "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
        "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
        "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
        "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
        "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
        "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
        "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
        "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
        "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
        "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
        "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
        "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
        "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
        "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
        "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
        "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
        "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
        "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
        "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
        "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
        "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
        "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2",
        "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
        "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
        "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93",
    };

    Raphael::SearchOptions options{.maxdepth = BENCH_DEPTH};
    engine.set_searchoptions(options);
    bool halt = false;
    cge::MouseInfo mouse = {.x = 0, .y = 0, .event = cge::MouseEvent::NONE};

    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bench: starting\n" << flush;
    }

    const auto start_t = ch::high_resolution_clock::now();
    for (auto fen : bench_data) {
        halt = false;
        const chess::Board board(fen);
        engine.get_move(board, 0, 0, mouse, halt);
    }

    const auto now = ch::high_resolution_clock::now();
    const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();


    lock_guard<mutex> lock(cout_mutex);
    cout << "\nbench: finished in " << dtime << "ms \n" << flush;
}
}  // namespace bench
}  // namespace raphael
