//! TIFF image reading and conversion utilities.
//!
//! This module provides functionality to read TIFF files and convert various
//! pixel formats to 8-bit grayscale for display.

use num_traits::cast::ToPrimitive;
use num_traits::float::Float;
use std::fs::File;
use tiff::decoder::{Decoder, DecodingResult};

use crate::image::Image;

/// Converts floating-point pixel data to 8-bit grayscale.
///
/// Normalizes the values to the [0, 1] range based on min/max values in the buffer,
/// then scales to [0, 255] for u8 representation.
///
/// # Arguments
///
/// * `buf` - Slice of floating-point pixel values
///
/// # Returns
///
/// Vector of 8-bit pixel values
fn to_u8_float<T>(buf: &[T]) -> Vec<u8>
where
    T: Float + ToPrimitive + Copy,
{
    if buf.is_empty() {
        return Vec::new();
    }

    let minv = buf.iter().copied().reduce(T::min).unwrap();
    let maxv = buf.iter().copied().reduce(T::max).unwrap();
    let span = if maxv != minv { maxv - minv } else { T::one() };
    let max_scaled = u8::MAX as f64;

    buf.iter()
        .copied()
        .map(|value| {
            let normalized = (value - minv) / span;
            let frac = normalized.to_f64().unwrap_or(0.0).clamp(0.0, 1.0);
            (frac * max_scaled) as u8
        })
        .collect()
}

/// Converts integer pixel data to 8-bit grayscale.
///
/// Normalizes the values to the [0, 1] range based on min/max values in the buffer,
/// then scales to [0, 255] for u8 representation.
///
/// # Arguments
///
/// * `buf` - Slice of integer pixel values
///
/// # Returns
///
/// Vector of 8-bit pixel values
fn to_u8_int<T>(buf: &[T]) -> Vec<u8>
where
    T: ToPrimitive + Copy + PartialOrd,
{
    if buf.is_empty() {
        return Vec::new();
    }

    let minv = buf.iter().copied().min_by(|a, b| a.partial_cmp(b).unwrap()).unwrap();
    let maxv = buf.iter().copied().max_by(|a, b| a.partial_cmp(b).unwrap()).unwrap();
    
    let min_f64 = minv.to_f64().unwrap_or(0.0);
    let max_f64 = maxv.to_f64().unwrap_or(0.0);
    let span = if max_f64 != min_f64 { max_f64 - min_f64 } else { 1.0 };

    buf.iter()
        .copied()
        .map(|value| {
            let val_f64 = value.to_f64().unwrap_or(0.0);
            let normalized = (val_f64 - min_f64) / span;
            (normalized * 255.0).clamp(0.0, 255.0) as u8
        })
        .collect()
}

/// Converts a decoded TIFF page to 8-bit pixel data.
///
/// Handles various pixel formats (U8, U16, U32, U64, F32, F64, I8, I16, I32, I64)
/// and converts them to normalized 8-bit values.
///
/// # Arguments
///
/// * `result` - The decoded TIFF page data
///
/// # Returns
///
/// Vector of 8-bit pixel values
fn page_to_u8(result: DecodingResult) -> Vec<u8> {
    match result {
        DecodingResult::U8(buf) => buf,
        DecodingResult::U16(buf) => to_u8_int(&buf),
        DecodingResult::U32(buf) => to_u8_int(&buf),
        DecodingResult::U64(buf) => to_u8_int(&buf),
        DecodingResult::F32(buf) => to_u8_float(&buf),
        DecodingResult::F64(buf) => to_u8_float(&buf),
        DecodingResult::I8(buf) => to_u8_int(&buf),
        DecodingResult::I16(buf) => to_u8_int(&buf),
        DecodingResult::I32(buf) => to_u8_int(&buf),
        DecodingResult::I64(buf) => to_u8_int(&buf),
    }
}

/// Reads a TIFF file and extracts all pages as Image objects.
///
/// Supports multi-page TIFF files and various pixel formats. Each page is
/// converted to 8-bit grayscale regardless of the original bit depth or format.
///
/// # Arguments
///
/// * `path` - Path to the TIFF file to read
///
/// # Returns
///
/// * `Ok(Vec<Image>)` - Vector of images, one for each page in the TIFF file
/// * `Err` - If the file cannot be opened, decoded, or is not a valid TIFF
///
/// # Examples
///
/// ```no_run
/// use tiffview::tifread::read_tiff;
///
/// let images = read_tiff("image.tif").expect("Failed to read TIFF");
/// println!("Loaded {} pages", images.len());
/// ```
pub fn read_tiff(path: &str) -> Result<Vec<Image>, Box<dyn std::error::Error>> {
    let fp = File::open(path)?;
    let mut decoder = Decoder::new(fp)?;
    let mut stack = Vec::<Image>::new();

    loop {
        let (_width, _height) = decoder.dimensions()?;

        let page = decoder.read_image()?;
        let decoded = page_to_u8(page);
        let img = Image::from_vec(_height as usize, _width as usize, decoded);
        stack.push(img);

        if decoder.next_image().is_err() {
            break;
        }
    }

    Ok(stack)
}
