fn main() {
    println!("=== Rust Loop Stress Test: 1,000 iterations ===");

    let mut sum = 0;
    for i in 0..1000 {
        sum += i;
    }

    println!("Sum of 0..999: {}", sum);
    let expected = 499500;
    println!("Expected: 499500");
    println!("Match: {}", sum == expected);
}
