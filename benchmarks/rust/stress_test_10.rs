fn main() {
    println!("=== Rust Loop Stress Test: 10 iterations ===");

    let mut sum = 0;
    for i in 0..10 {
        sum += i;
    }

    println!("Sum of 0..9: {}", sum);
    let expected = 45;
    println!("{}", if sum == expected { "✓ PASS" } else { "✗ FAIL" });
}
