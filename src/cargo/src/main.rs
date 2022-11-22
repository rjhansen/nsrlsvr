extern crate log;

use std::cmp::Ordering;
use log::{info, debug, warn, error};
use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::process::exit;
use std::path::Path;
use clap::{Parser};
mod globals;

#[derive(Parser)]
#[command(author = "Robert J. Hansen <rob@hansen.engineering>")]
#[command(version = globals::PACKAGE_VERSION)]
#[command(about = "nsrlsvr serves up MD5 hashes from the NIST NSRL RDS.", long_about = None)]
struct Cli {
    /// Show where to file bug reports
    #[arg(long)]
    #[arg(default_value_t = false)]
    bug_report: bool,

    /// Simulate operations without ever going online.
    #[arg(long)]
    #[arg(default_value_t = false)]
    dry_run: bool,

    /// Serve an alternate set of hashes
    #[arg(short, long, value_name="FILE")]
    #[arg(default_value_t = globals::PKGDATADIR.to_owned() + "/hashes.txt")]
    #[arg(value_parser = does_hash_file_exist)]
    file: String,

    /// Set port to listen on
    #[arg(short, long, value_name="PORT")]
    #[arg(value_parser = clap::value_parser!(u16).range(1..))]
    #[arg(default_value_t = 9120)]
    port: u16,
}

// Note: THIS DOES NOT VERIFY THE FILE WILL EXIST WHEN WE GO TO READ IT.
// THINKING IT DOES SO LEADS TO RACE CONDITIONS.  DON'T.  This is *only*
// a sanity check for bootstrapping nsrlsvr startup, nothing more.
fn does_hash_file_exist(s: &str) -> Result<String, String> {
    let entry = Path::new(s);
    match entry.exists() && entry.is_file() {
        true => Ok(s.to_string()),
        false => Err(format!("{} not found", s))
    }
}

fn load_hashes(filename: String) -> Vec<String> {
    let mut rv: Vec<String> = Vec::new();
    let re = match regex::Regex::new("^[A-Fa-f0-9]{32}$") {
        Ok(s) => s,
        Err(_) => {
            error!("couldn't compile static regex: WTF?");
            exit(-1);
        }
    };
    for line in BufReader::new(match File::open(filename) {
        Ok(s) => s,
        Err(_) => {
            error!("couldn't open hash file for reading!");
            exit(-1);
        }
    }).lines() {
        match line {
            Ok(s) => if re.is_match(&s) {
                rv.push(s.to_uppercase());
                if rv.len() % 1000000 == 0 {
                    debug!("{} hashes read", rv.len());
                }
            },
            Err(_) => {
                error!("error reading hash file!");
                exit(-1);
            }
        }
    }
    rv.sort();
    rv
}

fn binary_search(v: &Vec<String>, val: &String) -> bool {
    let mut low: usize = 0;
    let mut high: usize = v.len() - 1;
    let mut mid: usize = low + ((high - low) / 2);

    while low != high {
        if v[mid] == val {
            return true;
        }
        if v[mid] < val {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
        mid = low + ((high - low) / 2);
    }
    return false;
}

fn main() {
    env_logger::init();
    debug!("parsing command line options");
    let cli = Cli::parse();
    if cli.bug_report {
        println!("File bugs online at: {}", globals::PACKAGE_BUGREPORT);
        exit(0);
    }
    let hashes = load_hashes(cli.file);
    debug!("{} hashes loaded", hashes.len());
}
