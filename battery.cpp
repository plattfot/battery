#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>

enum Status : unsigned {
  // Connected to power and is not charging
  ST_POWER = 0,
  // Running on battery
  ST_DISCHARGING = 1,
  // Battery is charging
  ST_CHARGING = 2
};

struct BatteryData {
  std::string status = " ";
  int full_design = -1;
  int remaining = -1;
  int present_rate= -1;
  int voltage = -1;
  bool watt_as_unit = false;
};

void parseBattery( const std::string& path, BatteryData& data )
{
  std::ifstream file(path);
  if( file.is_open() )
    {
      std::string line;
      auto to_int = [&line](const size_t delim){
        return atoi( line.substr(delim+1).c_str() );
      };

      auto begins_with = [&line]( const char* txt, const size_t end ){
        return line.compare(0,end,txt) == 0;
      };
      
      while( std::getline( file, line ) )
        {
          // Based on the parser in i3status print_battery_info.c
          const size_t delim = line.find('=');
          if( begins_with("POWER_SUPPLY_ENERGY_NOW",delim) ) {
            data.watt_as_unit = true;
            data.remaining = to_int(delim);
          } else if( begins_with("POWER_SUPPLY_CHARGE_NOW", delim) ){
            data.watt_as_unit = false;
            data.remaining = to_int( delim );
          } else if( begins_with( "POWER_SUPPLY_CURRENT_NOW", delim) )
            data.present_rate = std::abs( to_int( delim ) );
          else if( begins_with( "POWER_SUPPLY_VOLTAGE_NOW", delim) )
            data.voltage = std::abs( to_int( delim ) );
        
          /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
           * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
           * unit instead of μAh. We will calculate it as we need it
           * later. */
          else if (begins_with("POWER_SUPPLY_POWER_NOW", delim ))
            data.present_rate = abs( to_int(delim) );
          else if (begins_with("POWER_SUPPLY_STATUS", delim )) {
            const std::string& status = line.substr(delim+1);
            if( status == "Charging")
              data.status = "";
            else if( status == "Discharging" )
              data.status = " ";
            else
              data.status = "";
          } else if (begins_with("POWER_SUPPLY_ENERGY_FULL_DESIGN", delim)){
            data.full_design = to_int( delim );
          }
        }
      file.close();
    }
}

int main(int argc, const char** argv )
{
  std::string path = "/sys/class/power_supply/BAT0";
  
  BatteryData data;
  parseBattery( path+"/uevent", data );
  // From print_battery_info.c in i3status
  /* the difference between POWER_SUPPLY_ENERGY_NOW and
   * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
   * given in mWh, the charge in mAh. So calculate every value given in
   * ampere to watt */
  if( !data.watt_as_unit ){
    data.present_rate = ((static_cast<double>(data.voltage) / 1000.0) *
                         (static_cast<double>(data.present_rate) / 1000.0));

    if( data.voltage != -1) {

      data.remaining = ((static_cast<double>(data.voltage) / 1000.0) *
                        (static_cast<double>(data.remaining) / 1000.0));

      data.full_design = ((static_cast<double>(data.voltage) / 1000.0) *
                          (static_cast<double>(data.full_design) / 1000.0));
    }
  }
  
  if( (data.full_design == -1) || (data.remaining == -1 ) ){
    std::cout<<"   "<<std::endl;
    return 33;
  }
  
  const double percentage = static_cast<double>(data.remaining)/
    static_cast<double>(data.full_design)*100.0;

  std::stringstream ss;
  ss<<data.status;

  if( percentage >= 95.0 ) {
    ss<<"  ";
  } else if( percentage >= 75.0 ) {
    ss<<"  ";
  } else if( percentage >= 50.0 ) {
    ss<<"  ";
  } else if( percentage >= 25.0 ) {
    ss<<"  ";
  } else {
    ss<<"  ";
  }

  std::cout<<ss.str()<<std::endl;
  std::cout<<ss.str()<<std::endl;

  if( percentage <= 10.0 )
    std::cout<<"#FF0000"<<std::endl;
  
  return 0;
}
