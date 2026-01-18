mod image;
mod tifread;
mod window;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    
    if args.len() < 2 {
        eprintln!("Usage: {} <tiff-file>", args[0]);
        std::process::exit(1);
    }
    
    let images = tifread::read_tiff(&args[1])?;
    window::run(images)?;
    
    Ok(())
}
