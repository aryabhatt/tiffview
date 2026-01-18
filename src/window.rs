use crate::image::Image;
use egui::{ColorImage, TextureHandle, TextureOptions};

pub struct ImageViewer {
    images: Vec<Image>,
    current_index: usize,
    texture: Option<TextureHandle>,
    scaled_image: Option<Image>,
    last_scale_factor: f32,
}

impl ImageViewer {
    pub fn new(images: Vec<Image>) -> Self {
        Self {
            images,
            current_index: 0,
            texture: None,
            scaled_image: None,
            last_scale_factor: 1.0,
        }
    }

    fn update_texture(&mut self, ctx: &egui::Context, scale_factor: f32) {
        if self.images.is_empty() {
            return;
        }

        // Only rescale if the scale factor changed significantly
        let needs_rescale = (scale_factor - self.last_scale_factor).abs() > 0.01;

        if needs_rescale || self.scaled_image.is_none() {
            let img = &self.images[self.current_index];
            self.scaled_image = Some(img.scale(scale_factor));
            self.last_scale_factor = scale_factor;
        }

        if let Some(scaled_img) = &self.scaled_image {
            let width = scaled_img.cols();
            let height = scaled_img.rows();

            // Convert grayscale u8 to RGBA
            let pixels: Vec<egui::Color32> = scaled_img
                .as_slice()
                .iter()
                .map(|&gray| egui::Color32::from_gray(gray))
                .collect();

            let color_image = ColorImage {
                size: [width, height],
                pixels,
            };

            self.texture = Some(ctx.load_texture("image", color_image, TextureOptions::LINEAR));
        }
    }

    fn navigate(&mut self, delta: isize) {
        if self.images.is_empty() {
            return;
        }

        let new_index =
            (self.current_index as isize + delta).rem_euclid(self.images.len() as isize) as usize;

        if new_index != self.current_index {
            self.current_index = new_index;
            self.texture = None;
            self.scaled_image = None;
        }
    }
}

impl eframe::App for ImageViewer {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Handle keyboard input
        if ctx.input(|i| i.key_pressed(egui::Key::ArrowRight)) {
            self.navigate(1);
        }
        if ctx.input(|i| i.key_pressed(egui::Key::ArrowLeft)) {
            self.navigate(-1);
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading(format!(
                "Image {} of {}",
                self.current_index + 1,
                self.images.len()
            ));

            if !self.images.is_empty() {
                let available_size = ui.available_size();
                let img = &self.images[self.current_index];
                let img_size = egui::vec2(img.cols() as f32, img.rows() as f32);

                // Calculate scale to fit window while maintaining aspect ratio
                let scale = (available_size.x / img_size.x).min(available_size.y / img_size.y);

                // Update texture with scaled image if needed
                if self.texture.is_none() || (scale - self.last_scale_factor).abs() > 0.01 {
                    self.update_texture(ctx, scale);
                }

                if let Some(texture) = &self.texture {
                    let response =
                        ui.add(egui::Image::new(texture).fit_to_exact_size(texture.size_vec2()));

                    // Handle mouse wheel scrolling
                    if response.hovered() {
                        let scroll = ui.input(|i| i.smooth_scroll_delta.y);
                        if scroll > 0.0 {
                            self.navigate(-1);
                        } else if scroll < 0.0 {
                            self.navigate(1);
                        }
                    }
                } else {
                    ui.label("Loading image...");
                }
            } else {
                ui.label("No images to display");
            }
        });
    }
}

pub fn run(images: Vec<Image>) -> Result<(), eframe::Error> {
    // Get the size of the first image to set initial window size
    let initial_size = if let Some(first_img) = images.first() {
        [first_img.cols() as f32, first_img.rows() as f32 + 30.0] // +30 for header
    } else {
        [800.0, 600.0]
    };

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size(initial_size)
            .with_title("tiffview"),
        multisampling: 0,
        ..Default::default()
    };

    eframe::run_native(
        "tiffview",
        options,
        Box::new(|_cc| Ok(Box::new(ImageViewer::new(images)))),
    )
}
