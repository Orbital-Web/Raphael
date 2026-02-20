use std::io::{self, Write};

// model params
const HIDDEN_SIZE: usize = 64;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

#[inline]
fn piece_map(p: char) -> usize {
    match p {
        'P' => 0,
        'N' => 1,
        'B' => 2,
        'R' => 3,
        'Q' => 4,
        'K' => 5,
        'p' => 6,
        'n' => 7,
        'b' => 8,
        'r' => 9,
        'q' => 10,
        'k' => 11,
        _ => panic!("Invalid piece character"),
    }
}

#[inline]
fn get_features(fen: &str) -> [Vec<usize>; 2] {
    let mut widx = Vec::new();
    let mut bidx = Vec::new();

    let mut parts = fen.splitn(3, ' ');
    let pieces = parts.next().unwrap();
    let side = parts.next().unwrap();

    let mut sq: u32 = 56;

    for p in pieces.chars() {
        match p {
            '/' => {
                sq -= 16;
            }
            '1'..='8' => {
                sq += p.to_digit(10).unwrap();
            }
            _ => {
                let wp = piece_map(p);
                let bp = (wp + 6) % 12;

                widx.push(64 * wp + sq as usize);
                bidx.push(64 * bp + (sq ^ 0x38) as usize);

                sq += 1;
            }
        }
    }

    match side {
        "w" => [widx, bidx],
        _ => [bidx, widx],
    }
}

#[repr(C)]
pub struct Network {
    w0: [i16; HIDDEN_SIZE * 768], // column major HIDDEN_SIZE x 768
    b0: [i16; HIDDEN_SIZE],
    w1: [i16; 2 * HIDDEN_SIZE], // column major 1 x (2 * HIDDEN_SIZE)
    b1: i16,
}

impl Network {
    pub fn evaluate(&self, fen: &str) -> i32 {
        let [stm_ft, ntm_ft] = get_features(&fen);

        let mut stm = self.b0;
        let mut ntm = self.b0;

        // fill accumulators
        for &idx in &stm_ft {
            let start = idx * HIDDEN_SIZE;
            let weights: &_ = &self.w0[start..start + HIDDEN_SIZE];
            for (a, &w) in stm.iter_mut().zip(weights.iter()) {
                *a += w;
            }
        }

        for &idx in &ntm_ft {
            let start = idx * HIDDEN_SIZE;
            let weights: &_ = &self.w0[start..start + HIDDEN_SIZE];
            for (a, &w) in ntm.iter_mut().zip(weights.iter()) {
                *a += w;
            }
        }

        // compute unscaled output
        let mut output: i32 = 0;

        for (&input, &weight) in stm.iter().zip(&self.w1[..HIDDEN_SIZE]) {
            let clamped = i32::from(input).clamp(0, i32::from(QA));
            output += (i32::from(weight) * clamped) * clamped;
        }

        for (&input, &weight) in ntm.iter().zip(&self.w1[HIDDEN_SIZE..]) {
            let clamped = i32::from(input).clamp(0, i32::from(QA));
            output += (i32::from(weight) * clamped) * clamped;
        }

        // scale based on quantization
        output = i32::from(QA) * i32::from(self.b1) + output;
        output /= i32::from(QA);
        output *= SCALE;
        output /= i32::from(QA) * i32::from(QB);

        output
    }
}

fn main() {
    let nnue: Network = unsafe { std::mem::transmute(*include_bytes!("../../net.nnue")) };

    let mut input = String::new();
    loop {
        print!("fen: ");
        io::stdout().flush().unwrap();

        input.clear();
        io::stdin()
            .read_line(&mut input)
            .expect("failed to read line");

        let fen = input.trim();
        if fen == "q" {
            break;
        }
        let eval = nnue.evaluate(fen);
        println!("eval: {}", eval);
    }
}
