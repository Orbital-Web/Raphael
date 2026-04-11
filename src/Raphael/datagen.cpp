#include <Raphael/datagen.h>

#include <csignal>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

using std::abs;
using std::atomic;
using std::cout;
using std::flush;
using std::ifstream;
using std::lock_guard;
using std::memory_order_relaxed;
using std::mt19937_64;
using std::mutex;
using std::ofstream;
using std::signal;
using std::string;
using std::thread;
using std::to_string;
using std::uniform_int_distribution;
using std::vector;
namespace ch = std::chrono;



namespace raphael::datagen {
namespace internal {
PackedBoard PackedBoard::pack(const chess::Board& board, i16 score) {
    // https://github.com/jnlt3/marlinflow/blob/main/marlinformat/src/lib.rs
    // https://github.com/Ciekce/Stormphrax/blob/main/src/datagen/marlinformat.h
    PackedBoard packed{};

    const auto cr = board.castle_rights();
    constexpr auto kside = chess::Board::CastlingRights::KING_SIDE;
    constexpr auto qside = chess::Board::CastlingRights::QUEEN_SIDE;
    constexpr u8 unmoved_rook_id = 6;

    auto occ = board.occ();
    packed.occ = static_cast<u64>(occ);

    i32 i = 0;
    while (occ) {
        const auto sq = static_cast<chess::Square>(occ.poplsb());
        const auto p = board.at(sq);

        u8 pt_id = static_cast<u8>(p.type());
        const u8 col_id = static_cast<u8>((p.color() == chess::Color::WHITE) ? 0 : (1 << 3));

        if (p.type() == chess::PieceType::ROOK
            && (sq == cr.get_rook_square(chess::Color::WHITE, kside)
                || sq == cr.get_rook_square(chess::Color::WHITE, qside)
                || sq == cr.get_rook_square(chess::Color::BLACK, kside)
                || sq == cr.get_rook_square(chess::Color::BLACK, qside)))
            pt_id = unmoved_rook_id;

        const u8 encoding = pt_id | col_id;
        assert(encoding <= 0xF);
        assert(i < 32);
        packed.pieces[i / 2] |= (i % 2 == 0) ? encoding : encoding << 4;
        i++;
    }

    const u8 stm_id = (board.stm() == chess::Color::WHITE) ? 0 : (1 << 7);
    const auto ep_id = static_cast<u8>(board.enpassant_square());
    packed.stm_enpassant = stm_id | ep_id;
    packed.halfmoves = board.halfmoves();
    packed.fullmoves = board.fullmoves();
    packed.eval = score;

    return packed;
}

ViriMove ViriMove::from_move(chess::Move move, i32 score) {
    ViriMove virimove;

    static constexpr u16 virimovetypes[4] = {
        static_cast<u16>(0x0000),  // normal
        static_cast<u16>(0xC000),  // promo
        static_cast<u16>(0x4000),  // enpassant
        static_cast<u16>(0x8000),  // castling
    };

    // encode move
    virimove.move |= static_cast<u16>(move.from());
    virimove.move |= static_cast<u16>(move.to()) << 6;
    virimove.move |= static_cast<u16>(move.promotion_type() - chess::PieceType::KNIGHT) << 12;
    virimove.move |= virimovetypes[move.type() >> 14];

    assert(score <= INT16_MAX);
    assert(score >= INT16_MIN);
    virimove.score = static_cast<i16>(score);

    return virimove;
}


void generation_thread(
    Raphael* engine,
    i32 softnodes,
    vector<string> seed_fens,
    string filename,
    u64 seed,
    i32 randmoves,
    bool dfrc
) {
    // create engines for both sides
    Raphael* engines[2] = {engine, nullptr};
    bool created_engine = false;
    if (engine == nullptr) {
        engines[0] = new Raphael("Raphael");
        created_engine = true;
    }
    engines[1] = new Raphael("Raphael");

    // initialize engines
    for (i32 i = 0; i < 2; i++) {
        engines[i]->set_uciinfolevel(raphael::Raphael::UciInfoLevel::NONE);
        engines[i]->set_option("Softnodes", true);
        engines[i]->set_option("Hash", DATAGEN_HASH);
        engines[i]->set_option("Threads", 1);
    }

    // initialize other stuff
    ofstream outfile(filename);
    mt19937_64 generator(seed);
    chess::Board board;
    board.set960(dfrc);
    chess::MoveList<chess::ScoredMove> movelist;
    Position<false> position;


    // keep generating upon wakeup
    while (true) {
        {
            lock_guard<mutex> gen_lock(gen_mutex);
            if (num_batch_remaining == 0) break;
            num_batch_remaining--;
        }

        // generate DATAGEN_BATCH_SIZE games
        i32 generated = 0;
        while (generated < DATAGEN_BATCH_SIZE) {
            // choose random seed_fen
            uniform_int_distribution<u64> distribution(0, seed_fens.size() - 1);
            board.set_fen(seed_fens[distribution(generator)]);

            // play random moves
            for (i32 m = 0; m < randmoves; m++) {
                movelist.clear();
                chess::Movegen::generate_legals(movelist, board);
                if (movelist.size() == 0) break;

                uniform_int_distribution<u64> distribution(0, movelist.size() - 1);
                const auto& move = movelist[distribution(generator)].move;
                board.make_move(move);
            }

            // filter unbalanced/illegal positions
            engines[0]->reset();
            engines[0]->set_board(board);
            const auto res = engines[0]->search({.maxnodes = GENFENS_MAX_NODES});
            if (res.move == chess::Move::NO_MOVE || res.is_mate
                || abs(res.score) > GENFENS_MAX_SCORE)
                continue;

            // play from here
            for (i32 i = 0; i < 2; i++) engines[i]->reset();
            position.set_board(board);

            auto packed = PackedBoard::pack(board, 0);
            vector<ViriMove> movescores;
            movescores.reserve(256);
            Outcome outcome = Outcome::INVALID;
            i32 winning_for = 0;
            i32 losing_for = 0;
            i32 drawing_for = 0;

            while (outcome == Outcome::INVALID) {
                const auto& curr_board = position.board();
                const auto stm = curr_board.stm();

                engines[stm]->set_position(position);
                const auto res = engines[stm]->search({.maxnodes = softnodes});
                auto abs_score = (stm == chess::Color::WHITE) ? res.score : -res.score;

                // handle terminal state
                if (res.move == chess::Move::NO_MOVE) {
                    if (curr_board.in_check())
                        outcome = (stm == chess::Color::WHITE) ? Outcome::BLACK_WIN
                                                               : Outcome::WHITE_WIN;
                    else
                        outcome = Outcome::DRAW;
                    break;
                }

                // reset draw adj counter on capture/pawn push
                if (curr_board.is_capture(res.move)
                    || curr_board.at(res.move.from()).type() == chess::PieceType::PAWN)
                    drawing_for = 0;

                // track adjudication
                if (res.is_mate) {
                    outcome = (abs_score > 0) ? Outcome::WHITE_WIN : Outcome::BLACK_WIN;
                    abs_score = (abs_score > 0) ? MATE_SCORE : -MATE_SCORE;
                } else {
                    if (abs_score > DATAGEN_WIN_ADJ_SCORE) {
                        winning_for++;
                        losing_for = 0;
                        drawing_for = 0;
                    } else if (abs_score < -DATAGEN_WIN_ADJ_SCORE) {
                        winning_for = 0;
                        losing_for++;
                        drawing_for = 0;
                    } else if (
                        curr_board.fullmoves() > DATAGEN_DRAW_ADJ_MVNUM
                        && abs(abs_score) < DATAGEN_DRAW_ADJ_SCORE
                    ) {
                        winning_for = 0;
                        losing_for = 0;
                        drawing_for++;
                    } else {
                        winning_for = 0;
                        losing_for = 0;
                        drawing_for = 0;
                    }

                    if (winning_for > DATAGEN_WIN_ADJ_MVCNT)
                        outcome = Outcome::WHITE_WIN;
                    else if (losing_for > DATAGEN_WIN_ADJ_MVCNT)
                        outcome = Outcome::BLACK_WIN;
                    else if (drawing_for > DATAGEN_DRAW_ADJ_MVCNT)
                        outcome = Outcome::DRAW;
                }

                position.make_move(res.move);

                // handle draws
                if (curr_board.is_halfmovedraw() || curr_board.is_insufficientmaterial()
                    || position.is_repetition())
                    outcome = Outcome::DRAW;

                movescores.push_back(ViriMove::from_move(res.move, abs_score));
            }

            // write to outfile
            static constexpr i32 null = 0;
            packed.outcome = outcome;
            outfile.write(reinterpret_cast<const char*>(&packed), sizeof(packed));
            outfile.write(
                reinterpret_cast<const char*>(movescores.data()),
                sizeof(ViriMove) * movescores.size()
            );
            outfile.write(reinterpret_cast<const char*>(&null), sizeof(ViriMove));
            generated++;
        }

        i32 n_games = num_games_generated.fetch_add(generated);
        n_games += generated;
        const auto delta
            = ch::duration_cast<ch::milliseconds>(ch::system_clock::now() - start_time).count();
        const auto games_persec = f64(n_games) * 1000.0 / delta;
        cout << "\rgenerated: " + to_string(n_games) + " games (" << games_persec << " games/sec)"
             << flush;
    }

    outfile.close();
    if (created_engine) delete engines[0];
    delete engines[1];
}


void handle_interrupt(i32) {
    lock_guard<mutex> gen_lock(gen_mutex);
    num_batch_remaining = 0;
}
}  // namespace internal



void generate_games(
    Raphael& main_engine,
    i32 softnodes,
    i32 games,
    const string& book,
    i32 randmoves,
    bool dfrc,
    i32 concurrency
) {
    signal(SIGINT, internal::handle_interrupt);

    // load seed_fens from book
    vector<string> seed_fens;
    if (book == "None")
        seed_fens.push_back(chess::Board::STARTPOS);
    else {
        ifstream file(book);
        if (!file) {
            cout << "info string could not open book: " << book << "\n" << flush;
            return;
        }

        string seed_fen;
        while (getline(file, seed_fen))
            if (!seed_fen.empty()) seed_fens.push_back(seed_fen);

        if (seed_fens.empty()) {
            cout << "info string book file is empty\n" << flush;
            return;
        }
        file.close();
    }

    // get current time to use as seed
    internal::start_time = ch::system_clock::now();
    const auto base_seed = static_cast<u64>(
        ch::duration_cast<ch::milliseconds>(internal::start_time.time_since_epoch()).count()
    );
    mt19937_64 generator(base_seed);
    uniform_int_distribution<u64> distribution(0, UINT64_MAX);

    // set total number of batches to run
    internal::num_batch_remaining = (games + DATAGEN_BATCH_SIZE - 1) / DATAGEN_BATCH_SIZE;
    cout << "starting generation of " + to_string(games) + " games with " << to_string(softnodes)
         << " softnodes, " << to_string(concurrency) << " threads\n"
         << "generated: 0 games (0.0000 games/sec)" << flush;

    // start generation threads
    thread threads[concurrency];
    for (int i = 0; i < concurrency; i++)
        threads[i] = thread(
            internal::generation_thread,
            (i == 0) ? &main_engine : nullptr,
            softnodes,
            seed_fens,
            to_string(base_seed) + "-" + to_string(i) + ".vf",
            distribution(generator),
            randmoves,
            dfrc
        );

    // wait for completion
    for (int i = 0; i < concurrency; i++) threads[i].join();

    cout << "\nfinished generation of " + to_string(internal::num_games_generated) + " games\n"
         << std::flush;
}
}  // namespace raphael::datagen
