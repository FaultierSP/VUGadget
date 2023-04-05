use sysinfo::{CpuExt,System, SystemExt};
//use chrono::prelude::*;
use configparser::ini::Ini;
use std::error::Error;
use serial2::SerialPort;
use another_json_minimal::*;

fn main() -> Result<(),Box<dyn Error>> {
    //Get settings
    let mut config = Ini::new();
    config.load("target/debug/config.ini")?;
    let config_sends_per_second:u64=config.getuint("transfer", "sends per second").unwrap().unwrap();
    let config_serial_device:String=config.get("transfer", "serial device").unwrap();
    
    //Init serial port
    let port=SerialPort::open(config_serial_device, 9600)?;
    
    //Init system data
    let mut sys=System::new_all();

    loop {
        let mut json=Json::new();
        //let dt: DateTime<Local>=Local::now();

        sys.refresh_cpu();

        let mut cpu_cores:u8=0;
        let mut percentage_per_core:f32=0.0;

        for cpu in sys.cpus(){
            cpu_cores+=1;
            percentage_per_core+=cpu.cpu_usage();
        }

        let cpu_json=Json::OBJECT{name:String::from("c"),value:Box::new(Json::STRING(
            ((percentage_per_core / cpu_cores as f32) as u8).to_string()
        ))};
        json.add(cpu_json);

        sys.refresh_memory();
        let mem_json=Json::OBJECT{name:String::from("r"),value:Box::new(Json::STRING(
            ((((sys.used_memory()/1048576) as f64 / (sys.total_memory()/1048576) as f64)*100.0) as u8).to_string()
        ))};
        json.add(mem_json);

        let string_json1=Json::OBJECT{name:String::from("s1"),value:Box::new(Json::STRING(
            String::from("String 1")))};
        json.add(string_json1);

        let string_json2=Json::OBJECT{name:String::from("s2"),value:Box::new(Json::STRING(
            String::from("String 2")))};
        json.add(string_json2);

        //Write to serial
        port.write(json.print().as_bytes())?;
        println!("{:?}",json.print());
        //println!("{:?}",String::from_utf8(port.read(&mut [0;256])));

        std::thread::sleep(std::time::Duration::from_millis(1000/config_sends_per_second));
    }
}
