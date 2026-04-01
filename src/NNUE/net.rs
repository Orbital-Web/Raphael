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

fn main() {
    // model params
    const NET_ID: &str = "hydra_v9";
    const HIDDEN_SIZE: usize = 1024;
    const NUM_OUTPUT_BUCKETS: usize = 8;
    const SCALE: f32 = 400.0;
    const QA: i16 = 255;
    const QB: i16 = 64;
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
    const NUM_INPUT_BUCKETS: usize = get_num_buckets(&BUCKET_LAYOUT);
    const _: () = assert!(NUM_INPUT_BUCKETS == 16);

    // hyperparams
    let dataset_path = "data/combined.vf";
    let superbatches = 600;
    let wdl_scheduler = wdl::LinearWDL { start: 0.2, end: 0.4 };
    let lr_scheduler = lr::Warmup {
        inner: lr::CosineDecayLR {
            initial_lr: 0.001,
            final_lr: 0.001 * 0.3f32.powi(5),
            final_superbatch: superbatches,
        },
        warmup_batches: 1600,
    };

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(BUCKET_LAYOUT))
        .output_buckets(MaterialCount::<NUM_OUTPUT_BUCKETS>)
        .save_format(&[
            SavedFormat::id("l0w")
                .transform(|store, weights| {
                    let factorizer = store.get("l0f").values.repeat(NUM_INPUT_BUCKETS);
                    weights.into_iter().zip(factorizer).map(|(a, b)| a + b).collect()
                })
                .round()
                .quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB).transpose(),
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            // input layer factorizer
            let l0f =
                builder.new_weights("l0f", Shape::new(HIDDEN_SIZE, 768), InitSettings::Zeroed);
            let expanded_factorizer = l0f.repeat(NUM_INPUT_BUCKETS);

            // input layer weights
            let mut l0 = builder.new_affine("l0", 768 * NUM_INPUT_BUCKETS, HIDDEN_SIZE);
            l0.weights = l0.weights + expanded_factorizer;

            // output layer weights
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, NUM_OUTPUT_BUCKETS);

            // inference
            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer).select(output_buckets)
        });

    // ensure abs(factorizer + weights) <= 1.98
    let stricter_clipping =
        AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", stricter_clipping);
    trainer.optimiser.set_params_for_weight("l0f", stricter_clipping);

    const BATCH_GLOM: usize = 8;
    let schedule = TrainingSchedule {
        net_id: NET_ID.to_string(),
        eval_scale: SCALE,
        steps: TrainingSteps {
            batch_size: 16_384 * BATCH_GLOM,
            batches_per_superbatch: 6104 / BATCH_GLOM,
            start_superbatch: 1,
            end_superbatch: superbatches,
        },
        wdl_scheduler: wdl_scheduler,
        lr_scheduler: lr_scheduler,
        save_rate: 10,
    };

    let settings = LocalSettings {
        threads: 2,
        test_set: None,
        output_directory: "checkpoints",
        batch_queue_size: 32,
    };

    let filter = Filter {
        min_pieces: 4,
        random_fen_skipping: true,
        random_fen_skip_probability: 0.5,
        ..Filter::default()
    };
    let dataloader = ViriBinpackLoader::new(&dataset_path, 4096, 6, filter);

    trainer.run(&schedule, &settings, &dataloader);
}
