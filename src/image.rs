//! Image data structure and manipulation utilities.
//!
//! This module provides a simple 2D image structure for storing and manipulating
//! grayscale image data with support for scaling and pixel access.

use std::ops::{Index, IndexMut};

/// A 2D grayscale image with row-major pixel storage.
///
/// The image stores pixel data in a flat vector where pixels are arranged in
/// row-major order (i.e., pixels[i * ncols + j] corresponds to pixel (i, j)).
#[derive(Clone, Debug)]
pub struct Image {
    /// Number of rows in the image
    nrows: usize,
    /// Number of columns in the image
    ncols: usize,
    /// Pixel data in row-major order
    pixels: Vec<u8>,
}

impl Image {
    /// Creates a new image with all pixels initialized to zero.
    ///
    /// # Arguments
    ///
    /// * `nrows` - Number of rows in the image
    /// * `ncols` - Number of columns in the image
    pub fn new(nrows: usize, ncols: usize) -> Self {
        let pixels = vec![0; nrows * ncols];
        Image {
            nrows,
            ncols,
            pixels,
        }
    }
    /// Creates an image from existing pixel data.
    ///
    /// # Arguments
    ///
    /// * `nrows` - Number of rows in the image
    /// * `ncols` - Number of columns in the image
    /// * `pixels` - Pixel data in row-major order
    ///
    /// # Panics
    ///
    /// Panics if `pixels.len()` does not equal `nrows * ncols`.
    pub fn from_vec(nrows: usize, ncols: usize, pixels: Vec<u8>) -> Self {
        assert_eq!(pixels.len(), nrows * ncols);
        Image {
            nrows,
            ncols,
            pixels,
        }
    }
    /// Returns the number of rows in the image.
    pub fn rows(&self) -> usize {
        self.nrows
    }

    /// Returns the number of columns in the image.
    pub fn cols(&self) -> usize {
        self.ncols
    }

    /// Returns the pixel data as a slice.
    pub fn as_slice(&self) -> &[u8] {
        &self.pixels
    }

    /// Scales the image by the given factor using bilinear interpolation.
    ///
    /// # Arguments
    ///
    /// * `factor` - Scaling factor (>1.0 enlarges, <1.0 shrinks)
    ///
    /// # Returns
    ///
    /// A new scaled image with dimensions `(nrows * factor, ncols * factor)`.
    pub fn scale(&self, factor: f32) -> Image {
        let w: usize = (self.ncols as f32 * factor) as usize;
        let h: usize = (self.nrows as f32 * factor) as usize;
        let mut new_pixels = vec![0; w * h];
        // bilinear interpolation
        for i in 0..h {
            for j in 0..w {
                let x = (i as f32) / factor;
                let y = (j as f32) / factor;
                let x0 = x.floor() as usize;
                let x1 = x0.min(self.nrows - 1);
                let y0 = y.floor() as usize;
                let y1 = y0.min(self.ncols - 1);
                let dx = x - (x0 as f32);
                let dy = y - (y0 as f32);
                let p00 = self[(x0, y0)] as f32;
                let p01 = self[(x0, y1)] as f32;
                let p10 = self[(x1, y0)] as f32;
                let p11 = self[(x1, y1)] as f32;
                let p0 = p00 * (1.0 - dy) + p01 * dy;
                let p1 = p10 * (1.0 - dy) + p11 * dy;
                let p = p0 * (1.0 - dx) + p1 * dx;
                new_pixels[i * w + j] = p.round() as u8;
            }
        }
        Image::from_vec(h, w, new_pixels)
    }
}

impl Index<(usize, usize)> for Image {
    type Output = u8;

    /// Indexes into the image at position (row, col).
    fn index(&self, (i, j): (usize, usize)) -> &Self::Output {
        let flat_idx = i * self.ncols + j;
        &self.pixels[flat_idx]
    }
}

impl IndexMut<(usize, usize)> for Image {
    /// Mutably indexes into the image at position (row, col).
    fn index_mut(&mut self, (i, j): (usize, usize)) -> &mut Self::Output {
        let flat_idx = i * self.ncols + j;
        &mut self.pixels[flat_idx]
    }
}
