// temperature_probe_parser.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"

#include "ns_dir.h"
#include "ns_ex.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
using namespace std;


struct ns_probe_measurement{
	ns_probe_measurement(){}
	ns_probe_measurement(const unsigned long time_, const float temp_, const char probe_name_):time(time_),temp(temp_),probe_name(probe_name_){}
	unsigned long time;
	float temp;
	char probe_name;
};
struct ns_input_file{
	ns_input_file(){}
	ns_input_file(const std::string & filename_,const char & probe_name_):filename(filename_),probe_name(probe_name_){}
	std::string filename;
	char probe_name;
	std::vector<ns_probe_measurement> measurements;
};

int main(int argc, char * argv[]){
	try{
		if (argc < 2)
			throw ns_ex("usage: ") << argv[0] << " [base filename] [output filename]\n";

		string base_filename(argv[1]);
		string output_filename_row(ns_dir::extract_filename_without_extension(base_filename) + "_processed_row.csv");
		string output_filename_column(ns_dir::extract_filename_without_extension(base_filename) + "_processed_column.csv");
	
		string base_dir(ns_dir::extract_path(base_filename));
		ns_dir dir;
		dir.load(base_dir);
		std::vector<ns_input_file> files;
		string base_filename_no_path(ns_dir::extract_filename(base_filename));
		for (unsigned int i = 0; i < dir.files.size(); i++){
			if (dir.files[i].substr(0,base_filename_no_path.size()) == base_filename_no_path &&
				dir.files[i].find("_processed_row.csv") == dir.files[i].npos &&
				dir.files[i].find("_processed_column.csv") == dir.files[i].npos
				)
				files.push_back(ns_input_file(base_dir + DIR_CHAR_STR + dir.files[i],  //filename
					*ns_dir::extract_filename_without_extension(dir.files[i]).rbegin()) //probe name is the last character in the filename
				);
		}
		cout << files.size() << " files identified.\n";
		if (files.empty())
			throw ns_ex("No files found.");
		for (unsigned int i = 0; i < files.size(); i++){
			ifstream in(files[i].filename);
			if (in.fail()) throw ns_ex("Could not load ") << files[i].filename;
			cout << "Reading " << files[i].filename << "...\n";
			std::string tmp;
			while(true){
				{
					const char a(in.peek());
					if (in.fail())
						break;
					if (a=='#'){  //comments
						getline(in,tmp);
						continue;
					}
				}
				std::string date;
				getline(in,date,',');
				if(in.fail())
					break;
				if (date.size() == 0)
					continue;
				std::string time;
				getline(in,time,',');
				std::string temperature;
				getline(in,temperature,',');
				files[i].measurements.push_back(ns_probe_measurement(ns_time_from_format_string(time + " " + date),
					atof(temperature.c_str()),files[i].probe_name));
				getline(in,tmp);
			}
			in.close();
		}
	
		//sort all measurements by time and group measurements made at the same time together.
		typedef std::map<unsigned long, std::vector<ns_probe_measurement> >  ns_measurement_aggregator;
		ns_measurement_aggregator measurement_aggregator;
		for (unsigned int i = 0; i < files.size(); i++){
			for (unsigned int j = 0; j < files[i].measurements.size(); j++){
				ns_measurement_aggregator::iterator p(measurement_aggregator.find(files[i].measurements[j].time));
				if (p == measurement_aggregator.end())
					p = measurement_aggregator.insert(measurement_aggregator.end(), ns_measurement_aggregator::value_type(files[i].measurements[j].time,std::vector<ns_probe_measurement>(files.size(),ns_probe_measurement(0,0,'.'))));
				(p->second)[i] = files[i].measurements[j];
			}
		}

		//output measurements by row (each measurement gets its own line)
		// and by column (each time gets all the measurements outputted)
		ofstream o_row(output_filename_row.c_str());
		ofstream o_column(output_filename_column.c_str());
		if (o_row.fail())
			throw ns_ex("Could not open output file: ") << output_filename_row;
		if (o_column.fail())
			throw ns_ex("Could not open output file: ") << output_filename_column;

		cout << "Writing processed data...\n";
		o_row << "Date and Time,Offset (Hours),Unix Time,Probe,Temperature\n";
		o_column << "Date and Time,Offset (Hours),Unix Time,";
		for (unsigned int i = 0; i < files.size(); i++){
			o_column << ",Probe " << files[i].probe_name << " Temperature";
		}
		o_column << "\n";

		for (ns_measurement_aggregator::const_iterator p(measurement_aggregator.begin()); p != measurement_aggregator.end(); p++){
			const std::string date(ns_format_time_string_for_tiff(p->first));
			const float offset_time((p->first - measurement_aggregator.begin()->first)/(60*60.0));
			o_column << date << "," << offset_time << "," <<p->first;
			for (unsigned int i = 0; i < p->second.size(); i++){
				if (p->second[i].temp!=0){
					o_row << date << "," << offset_time << "," << p->first << "," << p->second[i].probe_name << "," << p->second[i].temp << "\n";
					o_column << "," << p->second[i].temp;
				}
				else 
					o_column << ",";
			}
			o_column << "\n";
		}
		o_row.close();
		o_column.close();
		cout << "Done!\n";

	}
	catch(ns_ex & ex){
		cerr << ex.text() << "\n";
		char a;
		a = cin.get();
		return 1;
	}
	return 0;
}

