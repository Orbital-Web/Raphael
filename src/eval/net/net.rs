use bullet_lib::{
    game::{
        inputs::{SparseInputType, get_num_buckets},
        outputs::MaterialCount,
    },
    nn::{
        Shape,
        optimiser::{AdamW, AdamWParams},
    },
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
        settings::LocalSettings,
    },
    value::{ValueTrainerBuilder, loader::ViriBinpackLoader},
};

use viriformat::dataformat::Filter;

use crate::threat_inputs::ThreatInputs;

// fn piece_count_acceptance(board: &Board) -> f64 {
//     #[rustfmt::skip]
//     const DESIRED_DISTRIBUTION: [f64; 33] = [
//         0.018411966423, 0.020641545085, 0.022727271053,
//         0.024669162740, 0.026467201733, 0.028121406444,
//         0.029631758462, 0.030998276198, 0.032220941240,
//         0.033299772000, 0.034234750067, 0.035025893853,
//         0.035673184944, 0.036176641754, 0.036536245870,
//         0.036752015705, 0.036823932846, 0.036752015705,
//         0.036536245870, 0.036176641754, 0.035673184944,
//         0.035025893853, 0.034234750067, 0.033299772000,
//         0.032220941240, 0.030998276198, 0.029631758462,
//         0.028121406444, 0.026467201733, 0.024669162740,
//         0.022727271053, 0.020641545085, 0.018411966423,
//     ];

//     static PIECE_COUNT_STATS: [AtomicU64; 33] = zeroed();
//     static PIECE_COUNT_TOTAL: AtomicU64 = AtomicU64::new(0);

//     let pc = board.pieces.occupied().count() as usize;
//     let count = PIECE_COUNT_STATS[pc].fetch_add(1, Ordering::Relaxed) + 1;
//     let total = PIECE_COUNT_TOTAL.fetch_add(1, Ordering::Relaxed) + 1;
//     let frequency = count as f64 / total as f64;

//     // Calculate the acceptance probability for this piece count
//     let acceptance = 0.5 * DESIRED_DISTRIBUTION[pc] / frequency;
//     acceptance.clamp(0., 1.)
// }

fn main() {
    // model params
    const NET_ID: &str = "yogsothoth_v2";
    const L1_SIZE: usize = 768;
    const L2_SIZE: usize = 32;
    const L3_SIZE: usize = 32;
    const NUM_INPUT_BUCKETS: usize = 16;
    const NUM_OUTPUT_BUCKETS: usize = 8;
    const SCALE: f32 = 400.0;
    const QA: i16 = 255;
    const QB: i16 = 128;
    const QC: i32 = 64;
    #[rustfmt::skip]
    const BUCKET_LAYOUT: [usize; 32] = [
        0,  1,  2,  3,
        4,  5,  6,  7,
        8,  8,  9,  9,
        10, 10, 11, 11,
        12, 12, 13, 13,
        12, 12, 13, 13,
        14, 14, 15, 15,
        14, 14, 15, 15
    ];
    const _: () = assert!(get_num_buckets(&BUCKET_LAYOUT) == NUM_INPUT_BUCKETS);

    // hyperparams
    const SUPERBATCHES_STAGE0: usize = 800;
    const SUPERBATCHES_STAGE1: usize = 200;
    const DATASET_STAGE0: &str = "data/combined.vf";
    const DATASET_STAGE1: &str = "data/combined_ft.vf";
    const BATCH_GLOM: usize = 8;

    // error correction terms
    const L1_SHIFT: usize = 8;
    const L1_SHIFT_SCALE: f32 = QA as f32 / ((1 << L1_SHIFT) as f32);
    const I8_RANGE: f32 = i8::MAX as f32 / (QB as f32);
    const L1_RANGE: f32 = I8_RANGE * L1_SHIFT_SCALE * L1_SHIFT_SCALE;

    let inputs = ThreatInputs::new(BUCKET_LAYOUT);

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(inputs)
        .output_buckets(MaterialCount::<NUM_OUTPUT_BUCKETS>)
        .save_format(&[
            SavedFormat::id("l0w"), // l0w export done in Python
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w")
                .transform(|_, mut weights| {
                    for i in weights.iter_mut() {
                        *i /= L1_SHIFT_SCALE * L1_SHIFT_SCALE;
                    }
                    weights
                })
                .round()
                .quantise::<i8>(QB)
                .transpose(),
            SavedFormat::id("l1b").round().quantise::<i32>(QC * (1 << L1_SHIFT)),
            SavedFormat::id("l2w").round().quantise::<i32>(QC).transpose(),
            SavedFormat::id("l2b").round().quantise::<i32>(QC.pow(3)),
            SavedFormat::id("l3w").round().quantise::<i32>(QC).transpose(),
            SavedFormat::id("l3b").round().quantise::<i32>(QC.pow(4)),
        ])
        .build_custom(|builder, (stm_inputs, ntm_inputs, output_buckets), target| {
            // feature transformer
            let l0 = builder.new_affine("l0", inputs.num_inputs(), L1_SIZE);
            l0.init_with_effective_input_size(20000);

            // layerstack weights
            let l1 = builder.new_affine("l1", L1_SIZE, NUM_OUTPUT_BUCKETS * L2_SIZE);
            let l2 = builder.new_affine("l2", L2_SIZE, NUM_OUTPUT_BUCKETS * L3_SIZE);
            let l3 = builder.new_affine("l3", L3_SIZE, NUM_OUTPUT_BUCKETS);

            // inference
            let stm_hidden = l0.forward(stm_inputs).crelu().pairwise_mul();
            let ntm_hidden = l0.forward(ntm_inputs).crelu().pairwise_mul();
            let h1 = stm_hidden.concat(ntm_hidden);
            let h2 = l1.forward(h1).select(output_buckets).screlu();
            let h3 = l2.forward(h2).select(output_buckets).crelu();
            let output = l3.forward(h3).select(output_buckets);

            // loss
            let ones_l1_vec = builder.new_constant(Shape::new(1, L1_SIZE), &[1.0 / L1_SIZE as f32; L1_SIZE]);
            let reg_loss = ones_l1_vec.matmul(h1);
            let eval_loss = output.sigmoid().squared_error(target);
            let loss = eval_loss + 0.005 * reg_loss;

            (output, loss)
        });

    // ensure abs(l1w) <= L1_RANGE so that abs(QB*l1w / FT_SHIFT_SCALE^2) <= 127
    let l0_clip = AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    let l1_clip = AdamWParams { max_weight: L1_RANGE, min_weight: -L1_RANGE, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", l0_clip);
    trainer.optimiser.set_params_for_weight("l1w", l1_clip);

    let filter =
        Filter { min_pieces: 4, random_fen_skipping: true, random_fen_skip_probability: 0.5, ..Filter::default() };

    let schedule_stage0 = TrainingSchedule {
        net_id: NET_ID.to_string() + "_stage0",
        eval_scale: SCALE,
        steps: TrainingSteps {
            batch_size: 16_384 * BATCH_GLOM,
            batches_per_superbatch: 6104 / BATCH_GLOM,
            start_superbatch: 1,
            end_superbatch: SUPERBATCHES_STAGE0,
        },
        wdl_scheduler: wdl::LinearWDL { start: 0.2, end: 0.4 },
        lr_scheduler: lr::Warmup {
            inner: lr::CosineDecayLR {
                initial_lr: 0.001,
                final_lr: 0.001 * 0.3f32.powi(5),
                final_superbatch: SUPERBATCHES_STAGE0,
            },
            warmup_batches: 1600,
        },
        save_rate: 100,
    };
    let schedule_stage1 = TrainingSchedule {
        net_id: NET_ID.to_string() + "_stage1",
        eval_scale: SCALE,
        steps: TrainingSteps {
            batch_size: 16_384 * BATCH_GLOM,
            batches_per_superbatch: 6104 / BATCH_GLOM,
            start_superbatch: 1,
            end_superbatch: SUPERBATCHES_STAGE1,
        },
        wdl_scheduler: wdl::ConstantWDL { value: 0.6 },
        lr_scheduler: lr::LinearDecayLR { initial_lr: 1.0e-5, final_lr: 1.0e-7, final_superbatch: SUPERBATCHES_STAGE1 },
        save_rate: 100,
    };

    let settings = LocalSettings { threads: 2, test_set: None, output_directory: "checkpoints", batch_queue_size: 32 };

    trainer.run(
        &schedule_stage0,
        &settings,
        &ViriBinpackLoader::new(&DATASET_STAGE0.to_string(), 4096, 4, filter.clone()),
    );
    // trainer.load_from_checkpoint("checkpoints/cerberus_v1_stage0-800");
    trainer.run(
        &schedule_stage1,
        &settings,
        &ViriBinpackLoader::new(&DATASET_STAGE1.to_string(), 4096, 4, filter.clone()),
    );

    for fen in [
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    ] {
        let eval = trainer.eval(fen);
        println!("FEN: {fen}");
        println!("EVAL: {}", SCALE * eval);
    }
}

// the rest is taken from Stormphrax's training script

mod threat_inputs {
    use bullet_lib::game::{formats::bulletformat::ChessBoard, inputs};

    use montyformat::chess::{Attacks, Piece, Side};

    use crate::{offsets, threats::map_piece_threat};

    #[derive(Clone, Copy)]
    pub struct ThreatInputs {
        buckets: [usize; 64],
        total_features: usize,
    }

    impl ThreatInputs {
        pub const TOTAL_THREATS: usize = 2 * offsets::END;

        pub fn new(buckets: [usize; 32]) -> Self {
            let num_buckets = inputs::get_num_buckets(&buckets);

            let mut expanded = [0; 64];
            for (idx, elem) in expanded.iter_mut().enumerate() {
                *elem = buckets[(idx / 8) * 4 + [0, 1, 2, 3, 3, 2, 1, 0][idx % 8]];
            }

            let total_features = Self::TOTAL_THREATS + 768 * num_buckets + 768;

            Self { buckets: expanded, total_features }
        }
    }

    impl Default for ThreatInputs {
        fn default() -> Self {
            let total_features = Self::TOTAL_THREATS + 768 + 768;
            Self { buckets: [0; 64], total_features }
        }
    }

    impl inputs::SparseInputType for ThreatInputs {
        type RequiredDataType = ChessBoard;

        fn num_inputs(&self) -> usize {
            self.total_features
        }

        fn max_active(&self) -> usize {
            128 + 32
        }

        fn map_features<F: FnMut(usize, usize)>(&self, pos: &Self::RequiredDataType, mut f: F) {
            let get = |ksq| (if ksq % 8 > 3 { 7 } else { 0 }, 768 * self.buckets[usize::from(ksq)]);
            let (stm_flip, stm_bucket) = get(pos.our_ksq());
            let (ntm_flip, ntm_bucket) = get(pos.opp_ksq());

            #[rustfmt::skip]
            inputs::Chess768.map_features(pos, |stm, ntm| {
                f(
                    ThreatInputs::TOTAL_THREATS + stm ^ stm_flip,
                    ThreatInputs::TOTAL_THREATS + ntm ^ ntm_flip,
                );
                f(
                    ThreatInputs::TOTAL_THREATS + 768 + stm_bucket + (stm ^ stm_flip),
                    ThreatInputs::TOTAL_THREATS + 768 + ntm_bucket + (ntm ^ ntm_flip),
                );
            });

            let mut bbs = [0; 8];
            for (pc, sq) in pos.into_iter() {
                let pt = 2 + usize::from(pc & 7);
                let c = usize::from(pc & 8 > 0);
                let bit = 1 << sq;
                bbs[c] |= bit;
                bbs[pt] |= bit;
            }

            let mut stm_count = 0;
            let mut stm_feats = [0; 128];
            map_threat_features(bbs, |stm| {
                stm_feats[stm_count] = stm;
                stm_count += 1;
            });

            bbs.swap(0, 1);
            for bb in &mut bbs {
                *bb = bb.swap_bytes();
            }

            let mut ntm_count = 0;
            let mut ntm_feats = [0; 128];
            map_threat_features(bbs, |ntm| {
                ntm_feats[ntm_count] = ntm;
                ntm_count += 1;
            });

            assert_eq!(stm_count, ntm_count);

            for (&stm, &ntm) in stm_feats.iter().zip(ntm_feats.iter()).take(stm_count) {
                f(stm, ntm);
            }
        }

        fn shorthand(&self) -> String {
            todo!();
        }

        fn description(&self) -> String {
            todo!();
        }
    }

    fn map_bb<F: FnMut(usize)>(mut bb: u64, mut f: F) {
        while bb > 0 {
            let sq = bb.trailing_zeros() as usize;
            f(sq);
            bb &= bb - 1;
        }
    }

    fn flip_horizontal(mut bb: u64) -> u64 {
        const K1: u64 = 0x5555555555555555;
        const K2: u64 = 0x3333333333333333;
        const K4: u64 = 0x0f0f0f0f0f0f0f0f;
        bb = ((bb >> 1) & K1) | ((bb & K1) << 1);
        bb = ((bb >> 2) & K2) | ((bb & K2) << 2);
        ((bb >> 4) & K4) | ((bb & K4) << 4)
    }

    fn map_threat_features<F: FnMut(usize)>(mut bbs: [u64; 8], mut f: F) {
        // horiontal mirror
        let ksq = (bbs[0] & bbs[Piece::KING]).trailing_zeros();
        if ksq % 8 > 3 {
            for bb in bbs.iter_mut() {
                *bb = flip_horizontal(*bb);
            }
        };

        let mut pieces = [13; 64];
        for side in [Side::WHITE, Side::BLACK] {
            for piece in Piece::PAWN..=Piece::KING {
                let pc = 6 * side + piece - 2;
                map_bb(bbs[side] & bbs[piece], |sq| pieces[sq] = pc);
            }
        }

        let mut count = 0;

        let occ = bbs[0] | bbs[1];

        for side in [Side::WHITE, Side::BLACK] {
            let side_offset = offsets::END * side;
            let opps = bbs[side ^ 1];

            for piece in Piece::PAWN..Piece::KING {
                map_bb(bbs[side] & bbs[piece], |sq| {
                    let threats = match piece {
                        Piece::PAWN => Attacks::pawn(sq, side),
                        Piece::KNIGHT => Attacks::knight(sq),
                        Piece::BISHOP => Attacks::bishop(sq, occ),
                        Piece::ROOK => Attacks::rook(sq, occ),
                        Piece::QUEEN => Attacks::queen(sq, occ),
                        _ => unreachable!(),
                    } & occ;

                    count += 1;
                    map_bb(threats, |dest| {
                        let enemy = (1 << dest) & opps > 0;
                        if let Some(idx) = map_piece_threat(piece, sq, dest, pieces[dest], enemy) {
                            f(side_offset + idx);
                            count += 1;
                        }
                    });
                });
            }
        }
    }
}

mod threats {
    use montyformat::chess::Piece;

    use crate::{attacks, indices, offsets};

    pub fn map_piece_threat(piece: usize, src: usize, dest: usize, target: usize, enemy: bool) -> Option<usize> {
        match piece {
            Piece::PAWN => map_pawn_threat(src, dest, target, enemy),
            Piece::KNIGHT => map_knight_threat(src, dest, target),
            Piece::BISHOP => map_bishop_threat(src, dest, target),
            Piece::ROOK => map_rook_threat(src, dest, target),
            Piece::QUEEN => map_queen_threat(src, dest, target),
            Piece::KING => panic!(),
            _ => unreachable!(),
        }
    }

    fn below(src: usize, dest: usize, table: &[u64; 64]) -> usize {
        (table[src] & ((1 << dest) - 1)).count_ones() as usize
    }

    const fn offset_mapping<const N: usize>(a: [usize; N]) -> [usize; 12] {
        let mut res = [usize::MAX; 12];

        let mut i = 0;
        while i < N {
            res[a[i] - 2] = i;
            res[a[i] + 4] = i + N;
            i += 1;
        }

        res
    }

    fn target_is(target: usize, piece: usize) -> bool {
        target % 6 == piece - 2
    }

    fn map_pawn_threat(src: usize, dest: usize, target: usize, enemy: bool) -> Option<usize> {
        const MAP: [usize; 12] = offset_mapping([Piece::PAWN, Piece::KNIGHT, Piece::ROOK]);

        if MAP[target] == usize::MAX || (enemy && dest > src && target_is(target, Piece::PAWN)) {
            return None;
        }

        let id = if dest.abs_diff(src) == [9, 7][(dest > src) as usize] { 0 } else { 1 };
        let attack = 2 * (src % 8) + id - 1;
        let threat = offsets::PAWN + MAP[target] * indices::PAWN + (src / 8 - 1) * 14 + attack;
        Some(threat)
    }

    fn map_knight_threat(src: usize, dest: usize, target: usize) -> Option<usize> {
        const MAP: [usize; 12] = offset_mapping([Piece::PAWN, Piece::KNIGHT, Piece::BISHOP, Piece::ROOK, Piece::QUEEN]);

        if MAP[target] == usize::MAX || dest > src && target_is(target, Piece::KNIGHT) {
            return None;
        }

        let idx = indices::KNIGHT[src] + below(src, dest, &attacks::KNIGHT);
        let threat = offsets::KNIGHT + MAP[target] * indices::KNIGHT[64] + idx;
        Some(threat)
    }

    fn map_bishop_threat(src: usize, dest: usize, target: usize) -> Option<usize> {
        const MAP: [usize; 12] = offset_mapping([Piece::PAWN, Piece::KNIGHT, Piece::BISHOP, Piece::ROOK]);

        if MAP[target] == usize::MAX || dest > src && target_is(target, Piece::BISHOP) {
            return None;
        }

        let idx = indices::BISHOP[src] + below(src, dest, &attacks::BISHOP);
        let threat = offsets::BISHOP + MAP[target] * indices::BISHOP[64] + idx;
        Some(threat)
    }

    fn map_rook_threat(src: usize, dest: usize, target: usize) -> Option<usize> {
        const MAP: [usize; 12] = offset_mapping([Piece::PAWN, Piece::KNIGHT, Piece::BISHOP, Piece::ROOK]);

        if MAP[target] == usize::MAX || dest > src && target_is(target, Piece::ROOK) {
            return None;
        }

        let idx = indices::ROOK[src] + below(src, dest, &attacks::ROOK);
        let threat = offsets::ROOK + MAP[target] * indices::ROOK[64] + idx;
        Some(threat)
    }

    fn map_queen_threat(src: usize, dest: usize, target: usize) -> Option<usize> {
        const MAP: [usize; 12] = offset_mapping([Piece::PAWN, Piece::KNIGHT, Piece::BISHOP, Piece::ROOK, Piece::QUEEN]);

        if MAP[target] == usize::MAX || dest > src && target_is(target, Piece::QUEEN) {
            return None;
        }

        let idx = indices::QUEEN[src] + below(src, dest, &attacks::QUEEN);
        let threat = offsets::QUEEN + MAP[target] * indices::QUEEN[64] + idx;
        Some(threat)
    }
}

mod offsets {
    use super::indices;

    pub const PAWN: usize = 0;
    pub const KNIGHT: usize = PAWN + 6 * indices::PAWN;
    pub const BISHOP: usize = KNIGHT + 10 * indices::KNIGHT[64];
    pub const ROOK: usize = BISHOP + 8 * indices::BISHOP[64];
    pub const QUEEN: usize = ROOK + 8 * indices::ROOK[64];
    pub const END: usize = QUEEN + 10 * indices::QUEEN[64];
}

mod indices {
    use super::attacks;

    macro_rules! init_add_assign {
        (|$sq:ident, $init:expr, $size:literal | $($rest:tt)+) => {{
            let mut $sq = 0;
            let mut res = [{$($rest)+}; $size + 1];
            let mut val = $init;
            while $sq < $size {
                res[$sq] = val;
                val += {$($rest)+};
                $sq += 1;
            }

            res[$size] = val;

            res
        }};
    }

    pub const PAWN: usize = 84;
    pub const KNIGHT: [usize; 65] = init_add_assign!(|sq, 0, 64| attacks::KNIGHT[sq].count_ones() as usize);
    pub const BISHOP: [usize; 65] = init_add_assign!(|sq, 0, 64| attacks::BISHOP[sq].count_ones() as usize);
    pub const ROOK: [usize; 65] = init_add_assign!(|sq, 0, 64| attacks::ROOK[sq].count_ones() as usize);
    pub const QUEEN: [usize; 65] = init_add_assign!(|sq, 0, 64| attacks::QUEEN[sq].count_ones() as usize);
}

mod attacks {
    macro_rules! init {
        (|$sq:ident, $size:literal | $($rest:tt)+) => {{
            let mut $sq = 0;
            let mut res = [{$($rest)+}; $size];
            while $sq < $size {
                res[$sq] = {$($rest)+};
                $sq += 1;
            }
            res
        }};
    }

    const A: u64 = 0x0101_0101_0101_0101;

    const DIAGS: [u64; 15] = [
        0x0100_0000_0000_0000,
        0x0201_0000_0000_0000,
        0x0402_0100_0000_0000,
        0x0804_0201_0000_0000,
        0x1008_0402_0100_0000,
        0x2010_0804_0201_0000,
        0x4020_1008_0402_0100,
        0x8040_2010_0804_0201,
        0x0080_4020_1008_0402,
        0x0000_8040_2010_0804,
        0x0000_0080_4020_1008,
        0x0000_0000_8040_2010,
        0x0000_0000_0080_4020,
        0x0000_0000_0000_8040,
        0x0000_0000_0000_0080,
    ];

    pub const KNIGHT: [u64; 64] = init!(|sq, 64| {
        let n = 1 << sq;
        let h1 = ((n >> 1) & 0x7f7f_7f7f_7f7f_7f7f) | ((n << 1) & 0xfefe_fefe_fefe_fefe);
        let h2 = ((n >> 2) & 0x3f3f_3f3f_3f3f_3f3f) | ((n << 2) & 0xfcfc_fcfc_fcfc_fcfc);
        (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8)
    });

    pub const BISHOP: [u64; 64] = init!(|sq, 64| {
        let rank = sq / 8;
        let file = sq % 8;
        DIAGS[file + rank].swap_bytes() ^ DIAGS[7 + file - rank]
    });

    pub const ROOK: [u64; 64] = init!(|sq, 64| {
        let rank = sq / 8;
        let file = sq % 8;
        (0xFF << (rank * 8)) ^ (A << file)
    });

    pub const QUEEN: [u64; 64] = init!(|sq, 64| BISHOP[sq] | ROOK[sq]);
}
