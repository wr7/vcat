mod decimal;

pub fn parse_hertz(input: &str) -> Option<u64> {
    let input = input
        .chars()
        .filter(|c| !c.is_whitespace() && ![',', '_', '\''].contains(c))
        .map(|c| c.to_ascii_lowercase())
        .collect::<String>();

    if let Some(val) = input.strip_suffix("khz") {
        return val
            .parse::<decimal::FixedDecimalU64<3>>()
            .ok()
            .map(|v| v.get());
    }

    // This is stupid, but we'll still support it
    if let Some(val) = input.strip_suffix("mhz") {
        return val
            .parse::<decimal::FixedDecimalU64<6>>()
            .ok()
            .map(|v| v.get());
    }

    if let Some(val) = input.strip_suffix("hz") {
        return val.parse::<u64>().ok();
    }

    // Fall back to Hz if no unit is specified
    input.parse::<u64>().ok()
}
