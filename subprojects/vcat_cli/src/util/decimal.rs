use std::str::FromStr;

pub struct FixedDecimalU64<const FRACTIONAL_PLACES: u64>(u64);

impl<const FRACTIONAL_PLACES: u64> FixedDecimalU64<FRACTIONAL_PLACES> {
    const MULTIPLIER: u64 = 10u64.pow(FRACTIONAL_PLACES as u32);

    pub fn get(&self) -> u64 {
        self.0
    }
}

impl<const FRACTIONAL_PLACES: u64> FromStr for FixedDecimalU64<FRACTIONAL_PLACES> {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let (whole_part, fractional_part) = s.rsplit_once('.').ok_or(())?;

        let whole_part = whole_part
            .parse::<u64>()
            .map_err(|_| ())?
            .checked_mul(Self::MULTIPLIER)
            .ok_or(())?;

        let fractional_part =
            &fractional_part[..fractional_part.len().min(FRACTIONAL_PLACES as usize)];

        let fractional_part = fractional_part
            .parse::<u64>()
            .map_err(|_| ())?
            .checked_mul(
                10u64
                    .checked_pow(FRACTIONAL_PLACES as u32 - fractional_part.len() as u32)
                    .ok_or(())?,
            )
            .ok_or(())?;

        Ok(Self(whole_part.checked_add(fractional_part).ok_or(())?))
    }
}
