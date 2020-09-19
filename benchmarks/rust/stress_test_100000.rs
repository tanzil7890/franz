fn main() {
    println!("=== Rust Loop Stress Test: 100,000 iterations ===");

    let mut sum: i64 = 0;
    for i in 0..100000 {
        sum += i;
    }

    println!("Sum of 0..99999: {}", sum);
    println!("Expected: 4999950000");
    println!("Result: {}", sum);
}
