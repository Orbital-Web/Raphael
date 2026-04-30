use bullet_lib::{
    game::{
        inputs::{ChessBucketsMirrored, get_num_buckets},
        outputs::MaterialCount,
    },
    nn::{
        InitSettings, Shape,
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
    const NET_ID: &str = "cerberus_v1";
    const L1_SIZE: usize = 1024;
    const L2_SIZE: usize = 16;
    const L3_SIZE: usize = 32;
    const NUM_INPUT_BUCKETS: usize = 16;
    const NUM_OUTPUT_BUCKETS: usize = 8;
    const SCALE: f32 = 400.0;
    const QA: i16 = 255;
    const QB: i16 = 64; // TODO: try 128 and clip L1 weights to 0.99
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

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(BUCKET_LAYOUT))
        .output_buckets(MaterialCount::<NUM_OUTPUT_BUCKETS>)
        .save_format(&[
            SavedFormat::id("l0w")
                .transform(|store, weights| {
                    let factorizer = store.get("l0f").values.f32().repeat(NUM_INPUT_BUCKETS);
                    weights.into_iter().zip(factorizer).map(|(a, b)| a + b).collect()
                })
                .round()
                .quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i8>(QB).transpose(),
            SavedFormat::id("l1b"),
            SavedFormat::id("l2w").transpose(),
            SavedFormat::id("l2b"),
            SavedFormat::id("l3w").transpose(),
            SavedFormat::id("l3b"),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            // input layer factorizer
            let l0f = builder.new_weights("l0f", Shape::new(L1_SIZE, 768), InitSettings::Zeroed);
            let expanded_factorizer = l0f.repeat(NUM_INPUT_BUCKETS);

            // input layer weights
            let mut l0 = builder.new_affine("l0", 768 * NUM_INPUT_BUCKETS, L1_SIZE);
            l0.init_with_effective_input_size(32);
            l0.weights = l0.weights + expanded_factorizer;

            // layerstack weights
            let l1 = builder.new_affine("l1", L1_SIZE, NUM_OUTPUT_BUCKETS * L2_SIZE);
            let l2 = builder.new_affine("l2", L2_SIZE, NUM_OUTPUT_BUCKETS * L3_SIZE);
            let l3 = builder.new_affine("l3", L3_SIZE, NUM_OUTPUT_BUCKETS);

            // inference
            let stm_hidden = l0.forward(stm_inputs).crelu().pairwise_mul();
            let ntm_hidden = l0.forward(ntm_inputs).crelu().pairwise_mul();
            let h1 = stm_hidden.concat(ntm_hidden);
            let h2 = l1.forward(h1).select(output_buckets).screlu();
            let h3 = l2.forward(h2).select(output_buckets).screlu();
            l3.forward(h3).select(output_buckets)
        });

    // ensure abs(l0w + l0f) <= 1.98 so that QA*(QB*l0w_merged) = QA*(QB*1.98) <= 32767
    // we also need to ensure abs(QB*l1w) <= 127, default clip of 1.98 handles that for l1
    // since l2 and l3 are float inference, we can remove the default clipping
    let l0_clip = AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    let no_clip = AdamWParams { max_weight: 128.0, min_weight: -128.0, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", l0_clip);
    trainer.optimiser.set_params_for_weight("l0f", l0_clip);
    trainer.optimiser.set_params_for_weight("l2w", no_clip);
    trainer.optimiser.set_params_for_weight("l2b", no_clip);
    trainer.optimiser.set_params_for_weight("l3w", no_clip);
    trainer.optimiser.set_params_for_weight("l3b", no_clip);

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
        &ViriBinpackLoader::new(&DATASET_STAGE0.to_string(), 4096, 6, filter.clone()),
    );
    // trainer.load_from_checkpoint("checkpoints/cerberus_v1_stage0-800");
    trainer.run(
        &schedule_stage1,
        &settings,
        &ViriBinpackLoader::new(&DATASET_STAGE1.to_string(), 4096, 6, filter.clone()),
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
