fn main() {
    println!("=== Rust Loop Stress Test: 10,000 iterations ===");

    let mut sum = 0;
    for i in 0..10000 {
        sum += i;
    }

    println!("Sum of 0..9999: {}", sum);
    let expected = 49995000;
    println!("Expected: 49995000");
    println!("Match: {}", sum == expected);
}
