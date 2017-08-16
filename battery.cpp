/*
  Copyright 2015 Fredrik Salomonsson

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
    
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cmath> // fabs
#include <array>
#include <vector>
#include <memory>
#include <regex>
#include <type_traits>

#include <getopt.h>
#include <stdlib.h>
#include <dirent.h>

enum class Status : size_t {
  DISCHARGING = 0,
  CHARGING = 1,
  PLUG = 2
};

static const std::string STATUS_ICONS[] = {
  " ",
  "",
  ""
};

typedef std::array<std::string,5> ArrayT;
struct Parameters 
{
  std::string path = "/sys/class/power_supply";
  ArrayT icons = ArrayT{ "", "", "", "", "" };
  long int battery = -1; // -1 sum all.
  double threshold = 10.0;
};

struct BatteryData {
  Status status = ST_PLUG;
  double full_design = -1.0;
  double full = -1.0;
  double remaining = -1.0;
  double present_rate= -1.0;
  double voltage = -1.0;
  bool watt_as_unit = false;
};

typedef std::vector<BatteryData> BatteryVectorT;

BatteryData combineBatteries( const BatteryVectorT& batteries )
{
  BatteryData data;
  data.remaining = 0.0;
  data.full_design = 0.0;
  data.full = 0.0;
  for( const auto& battery : batteries ) {
    data.status = std::min( data.status, battery.status );
    data.remaining += battery.remaining;
    data.full_design += battery.full_design;
    data.full += battery.full;
    data.present_rate = std::max( data.present_rate, battery.present_rate );
    data.voltage = std::max( data.voltage, battery.voltage );
  }
  return data;
}

size_t countBatteries(const std::string& path) 
{
  DIR* dir;
  struct dirent *ent;

  size_t count = 0;
  if( (dir = opendir( path.c_str() )) != NULL ) {
    std::string file;
    std::regex exp("BAT[0-9]+");
    
    while( ( ent = readdir(dir) ) != NULL ) {
      count += std::regex_match( ent->d_name, exp );
    }
    closedir(dir);
  }
  return count;
}

void parseBattery( const std::string& path, BatteryData& data )
{
  std::ifstream file(path);
  if( file.is_open() )
    {
      std::string line;
      auto to_double = [&line](const size_t delim){
        return static_cast<double>( atoi( line.substr(delim+1).c_str() ) );
      };

      auto begins_with = [&line]( const char* txt, const size_t end ){
        return line.compare(0,end,txt) == 0;
      };
      
      while( std::getline( file, line ) ) {
        // Based on the parser in i3status print_battery_info.c
        const size_t delim = line.find('=');
        if( begins_with("POWER_SUPPLY_ENERGY_NOW",delim) ) {
          data.watt_as_unit = true;
          data.remaining = to_double(delim);
        } else if( begins_with("POWER_SUPPLY_CHARGE_NOW", delim) ){
          data.watt_as_unit = false;
          data.remaining = to_double( delim );
        } else if( begins_with( "POWER_SUPPLY_CURRENT_NOW", delim) )
          data.present_rate = std::fabs( to_double( delim ) );
        else if( begins_with( "POWER_SUPPLY_VOLTAGE_NOW", delim) )
          data.voltage = std::fabs( to_double( delim ) );
        
        /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
         * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
         * unit instead of μAh. We will calculate it as we need it
         * later. */
        else if (begins_with("POWER_SUPPLY_POWER_NOW", delim ))
          data.present_rate = abs( to_double(delim) );
        else if (begins_with("POWER_SUPPLY_STATUS", delim )) {
          const std::string& status = line.substr(delim+1);
          if( status == "Charging" ) {
            data.status = ST_CHARGING;
          } else if( status == "Discharging" ) {
            data.status = ST_DISCHARGING;
          } else {
            data.status = ST_PLUG;
          }
        } else if (begins_with("POWER_SUPPLY_ENERGY_FULL_DESIGN", delim)){
          data.full_design = to_double( delim );
        } else if (begins_with("POWER_SUPPLY_ENERGY_FULL", delim)){
          data.full = to_double( delim );
        }
      }
      file.close();
    }
}

void convertTo_mWh( BatteryData& data )
{
  // From print_battery_info.c in i3status
  if( data.voltage != -1) {
    const double mV = data.voltage / 1000.0;  

    data.present_rate = mV * (data.present_rate / 1000.0);
    data.remaining = mV * (data.remaining / 1000.0);
    data.full_design = mV * (data.full_design / 1000.0);
    data.full = mV * (data.full / 1000.0);
  }
}

void computeTime( const BatteryData& data, std::stringstream& ss )
{
  double remaining_time;

  if( data.status == ST_CHARGING) {
    remaining_time = (data.full - data.remaining) / data.present_rate;
  } else if ( data.status == ST_DISCHARGING ) {
    remaining_time = data.remaining / data.present_rate;
  } else {
    remaining_time = 0.0;
  }
    
  const size_t seconds_remaining = static_cast<size_t>( remaining_time * 3600.0 );

  const size_t hours = seconds_remaining / 3600;
  const size_t seconds = seconds_remaining - (hours * 3600);
  const size_t minutes = seconds / 60;
  
  ss<<(hours<10?"0":"")<<hours<<":"<<(minutes<10?"0":"")<<minutes;
}

bool parseArgs( const int argc, char* const* argv, Parameters& params )
{
  struct option long_options[] =
    {
      {"type",  required_argument, 0, 't'},
      {"custom",  required_argument, 0, 'c'},
      {"help",  no_argument, 0, 'h'},
      {"battery",  no_argument, 0, 'b'},
      {"path", required_argument, 0, 'p'},
      {"threshold", required_argument, 0, 'T'},
      {0, 0, 0, 0}
    };

  static const std::string help_text = R"(
Options:
  -t, --type [TYPE]         Specify what icons to use to indicate 
                            the battery status. Select between battery or
                            heart. Battery is the default.
  -c, --custom [FULL,EMPTY] Use custom battery indicator, using a 
                            combination  of characters FULL and EMPTY. 
  -b, --battery [INT]       Specify what battery to monitor. -1 combines
                            all the batteries and is the default.
  -T, --threshold [INT]     When the battery percentage falls under this 
                            threshold it will color the block red.
                            Default is 10.
  -p, --path [PATH]         Specify path to where info on the batteries are 
                            stored, default is /sys/class/power_supply.
  -h, --help                Print this message and then exit.
Author:
  Fredrik "PlaTFooT" Salomonsson
)";
  int option_index = 0;
  int c;
  while( true ) {
    c = getopt_long (argc, argv, "t:c:b:T:p:h", long_options, &option_index);
    
    /* Detect the end of the options. */
    if (c == -1)
      break;
      
    switch(c) {
    case 't':
      {
        std::string arg(optarg);
        if( arg == "heart") {
          params.icons = 
            {"", "", "", "", ""};
        } 
      }
      break;
    case 'c':
      {
        const std::string arg = optarg;
        const size_t delim = arg.find(',');

        if( delim == std::string::npos ) {
          std::cerr<<"[ERROR] Need to delimit FULL and EMPTY with a ','"<<std::endl;
          return false;
        }
        const std::string full = arg.substr(0,delim);
        const std::string empty = arg.substr(delim+1);
        params.icons = 
          { full  + full  + full  + full,
            full  + full  + full  + empty,
            full  + full  + empty + empty,
            full  + empty + empty + empty,
            empty + empty + empty + empty };
      }
      break;
    case 'b':
      // Using strol for more robust conversion than atoi
      params.battery = strtol(optarg,NULL,10);
      break;
    case 'p':
      params.path = optarg;
      break;
    case 'T':
      params.threshold = static_cast<double>( strtol( optarg, NULL, 10 ) );
      break;
    case 'h':
      std::cout<<"Usage: "<<argv[0]<<" [OPTIONS]..."<<help_text;
      return false;
      break;
    }
  }
  return true;
}

size_t processButtons( const BatteryVectorT& batteries,
                     const size_t num_batts,
                     Parameters& params )
{
  std::string block_button;
  {
    char* block_buffer = getenv("BLOCK_BUTTON");
    block_button = block_buffer != NULL ? std::move( block_buffer ) : "";
  }

  if( !block_button.empty() ){
    // Handle button event
    if( block_button == "1" )
      return 1;
    // Button 2 and 3 only valid if combining all the batteries.
    else if( params.battery == -1 ){
      const size_t button = strtoul( block_button.c_str(), NULL, 10 );
      // Find the first battery with energy remaining, starting from
      // the back.
      for( auto it = batteries.crbegin(), end = batteries.crend();
           it != end; ++it ){
        if( it->remaining > 0.0 ) {
          params.battery = (end - it) - 1;
          return button;
        }
      }
    }
  }
  return 0;
}

int main( int argc, char** argv )
{
  Parameters params;

  if( !parseArgs(argc,argv,params) )
    return 1;

  const long int num_batts = countBatteries( params.path );
  // Make sure battery index is within bounds
  params.battery = std::min( params.battery, num_batts - 1 );

  std::vector<BatteryData> batteries(num_batts);
  
  for( size_t i = 0; i < num_batts; ++i ) {
    BatteryData& data = batteries[i];

    std::stringstream batt_path;
    batt_path<<params.path<<"/BAT"<<i<<"/uevent";
    parseBattery( batt_path.str(), data );
  
    /* The difference between POWER_SUPPLY_ENERGY_NOW and
     * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
     * given in mWh, the charge in mAh. So calculate every value given
     * in ampere  to watt */
    if( !data.watt_as_unit )
      convertTo_mWh(data);
  
    if( (data.full_design < 0.0) || (data.remaining < 0.0 ) ){
      std::cout<<"  "<<std::endl;
      return 1;
    }
  }

  // Returns 0 if no button is pressed
  const size_t button = processButtons( batteries, num_batts, params );

  const BatteryData& data = params.battery == -1 
    ? combineBatteries( batteries ) 
    : batteries[params.battery];
 
  const double percentage = data.remaining / data.full_design * 100.0;

  std::stringstream ss;
  switch( button ) {
  case 2:
  case 3:
    ss<<params.battery<<":";
    break;
  default:
    ss<<STATUS_ICONS[ data.status ]<<" ";
  }
  
  // Add battery status 
  switch( button ) {
  case 1:
  case 2:
    ss<<static_cast<size_t>(percentage)<<"% ";
    break;
  default:
      {
        if( percentage >= 95.0 ) {
          ss<<params.icons[0];
        } else if( percentage >= 75.0 ) {
          ss<<params.icons[1];
        } else if( percentage >= 50.0 ) {
          ss<<params.icons[2];
        } else if( percentage >= 25.0 ) {
          ss<<params.icons[3];
        } else {
          ss<<params.icons[4];
        }
        ss<<" ";
      }
  }
  
  // Compute time of discharge/charge
  if( data.present_rate > 0.0 ) {
    computeTime( data, ss );
  } else {
    ss<<"Full";
  }

  // Output the format i3blocks want
  std::cout<<ss.str()<<std::endl;
  std::cout<<ss.str()<<std::endl;

  if( percentage <= params.threshold )
    std::cout<<"#FF0000"<<std::endl;
  
  return 0;
}
