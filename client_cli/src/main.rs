use sysinfo::{CpuExt,System,SystemExt,NetworkExt};
use configparser::ini::Ini;
use std::error::Error;
use serial2::SerialPort;
use another_json_minimal::*;

fn main() -> Result<(),Box<dyn Error>> {
    //Get settings
    let mut config = Ini::new();
    config.load("config.ini")?;
    let config_sends_per_second:u64=config.getuint("transfer", "sends per second").unwrap().unwrap();
    let config_serial_device:String=config.get("transfer", "serial device").unwrap();
    let config_network_interface:String=config.get("transfer","network interface").unwrap();
    let config_network_refresh_rate:u64=config.getuint("transfer","network stats refresh rate per second").unwrap().unwrap();
    
    //Init serial port
    let port=SerialPort::open(config_serial_device, 9600)?;
    
    //Init system data
    let mut sys=System::new_all();

    //Define variables
    let mut bytes_recieved:u64=0;
    let mut bytes_transmitted:u64=0;
    let mut loop_counter:u64=1;
    let mut network_was_inactive:bool=false;

    loop {
        let mut json=Json::new();

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

        sys.refresh_networks();

        for (interface_name,data) in sys.networks() {
            if interface_name==&config_network_interface {
                if (config_sends_per_second/config_network_refresh_rate) != loop_counter {
                    loop_counter+=1;

                    bytes_recieved+=data.received();
                    bytes_transmitted+=data.transmitted();
                }
                else {
                    if !(bytes_recieved==0 && network_was_inactive) {
                        let bytes_recieved_json=Json::OBJECT { name:String::from("br"),value:Box::new(Json::NUMBER((
                            (bytes_recieved*config_network_refresh_rate)/1024
                        ) as f64))};
                        json.add(bytes_recieved_json);
                    }
                    
                    if !(bytes_transmitted==0 && network_was_inactive) {
                        let bytes_transmitted_json=Json::OBJECT { name:String::from("bt"),value:Box::new(Json::NUMBER((
                            (bytes_transmitted*config_network_refresh_rate)/1024
                        ) as f64))};
                        json.add(bytes_transmitted_json);
                    }
                   
                    if (bytes_recieved==0 && bytes_transmitted==0) {
                        network_was_inactive=true;
                    }
                    else {
                        network_was_inactive=false;
                    }

                    loop_counter=1;
                    bytes_recieved=0;
                    bytes_transmitted=0;
                }
            }
        }

        /*let string_json1=Json::OBJECT{name:String::from("s1"),value:Box::new(Json::STRING(
            String::from("String das ein bisschen Länger ist.")))};
        json.add(string_json1);

        let string_json2=Json::OBJECT{name:String::from("s2"),value:Box::new(Json::STRING(
            String::from("String 2")))};
        json.add(string_json2);*/

        //Write to serial
        port.write(json.print().as_bytes())?;
        //println!("{:?}",json.print());

        std::thread::sleep(std::time::Duration::from_millis(1000/config_sends_per_second));
    }
}
