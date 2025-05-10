namespace Raphael {
struct v2_0_params {
    v2_0_params() {}

    // logic
    static constexpr int ASPIRATION_WINDOW = 50;  // size of aspiration window
    static constexpr int MAX_EXTENSIONS = 16;     // max number of times we can extend the search
    static constexpr int REDUCTION_FROM = 5;      // from which move to apply late move reduction
    static constexpr int PV_STABLE_COUNT = 6;     // consecutive bestmoves to consider pv as stable
    static constexpr int MIN_SKIP_EVAL = 200;     // minimum eval to halt early if pv is stable

    // move ordering
    static constexpr int KILLER_WEIGHT = 50;          // move ordering priority for killer moves
    static constexpr int GOOD_CAPTURE_WEIGHT = 5000;  // move ordering priority for good captures

    // evaluation[midgame, endgame]
    static constexpr int PVAL[12][2] = {
        // WHITE
        {100,   100 }, // PAWN
        {418,   246 }, // KNIGHT
        {449,   274 }, // BISHOP
        {554,   437 }, // ROOK
        {1191,  727 }, // QUEEN
        {0,     0   }, // KING
        // BLACK
        {-100,  -100}, // PAWN
        {-418,  -246}, // KNIGHT
        {-449,  -274}, // BISHOP
        {-554,  -437}, // ROOK
        {-1191, -727}, // QUEEN
        {0,     0   }, // KING
    };  // value of each piece
};
}  // namespace Raphael
