fn main() {
    println!("=== Rust Loop Stress Test: 100 iterations ===");

    let mut sum = 0;
    for i in 0..100 {
        sum += i;
    }

    println!("Sum of 0..99: {}", sum);
    let expected = 4950;
    println!("Expected: 4950");
    println!("Match: {}", sum == expected);
}
