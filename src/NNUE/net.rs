use bullet_lib::{
    game::inputs::Chess768,
    nn::optimiser::AdamW,
    trainer::{
        save::SavedFormat,
        schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
        settings::LocalSettings,
    },
    value::{loader::ViriBinpackLoader, ValueTrainerBuilder},
};

use viriformat::dataformat::Filter;

fn main() {
    // model params
    const NET_ID: &str = "basilisk_v3";
    const HIDDEN_SIZE: usize = 64;
    const SCALE: f32 = 400.0;
    const QA: i16 = 255;
    const QB: i16 = 64;

    // hyperparams
    let dataset_path = "data/full.vf";
    let superbatches = 60;
    let wdl_scheduler = wdl::ConstantWDL { value: 0.4 };
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
        .inputs(Chess768)
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB),
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs| {
            // weights
            let l0 = builder.new_affine("l0", 768, HIDDEN_SIZE);
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, 1);

            // inference
            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer)
        });

    let schedule = TrainingSchedule {
        net_id: NET_ID.to_string(),
        eval_scale: SCALE,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104,
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
        min_pieces: 2,
        // TODO: try wdl filtering
        ..Filter::default()
    };
    let dataloader = ViriBinpackLoader::new(&dataset_path, 512, 6, filter);

    trainer.run(&schedule, &settings, &dataloader);
}
