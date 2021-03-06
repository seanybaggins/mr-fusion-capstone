%% File: Parse_lidar_bruder_31min.m
% Author :Emmett Hamman
% EE 421: Mr. Fusion Capstone
% Description: Script will parse .csv file of 31 min VN200 log file and
%              save the Accel, Gyro, and GPS data
%              NOTE: Does not save the checksum from each sample
%              NOTE: IMU Fs = 50 Hz; GPS = 5 Hz.

clear;
close all;
clc;

% Preallocate Space
vn200_data = zeros(41822, 14);
vn200_txt_tag = zeros(41822, 1);
vn200_accel = zeros(38020, 3);
vn200_gyro = zeros(38020, 3);
vn200_gps_ECEF_xyz = zeros(3802, 3);
% vn200_baro = zeros(38020, 1);
% vn200_mag_compass = zeros(38020, 3);

%% Read Data in from File, store to temp variables
% Change xlsread range to include more data; currently reads in 120 sec of 
% IMU and GPS data

file_name = strcat('.\lidar_bruder_31min.log.csv');
[vn200_data, vn200_txt_tag] = xlsread(file_name, 1, 'A60002:O66601');


%% Parse through data in temp variables
vn200_accel = zeros(1,3);
vn200_gyro = zeros(1,3);
vn200_gps_ECEF_xyz = zeros(1,3);
vn200_gps_ECEF_vel = zeros(1,3);
vn200_gps_Fs = 5;
vn200_imu_Fs = 50;

for i = 1:length(vn200_txt_tag)
    if vn200_txt_tag(i,1) == "$VNIMU"
        if size(vn200_accel,1) == 1 && vn200_accel(1,1) == 0
          vn200_accel(1, :) = vn200_data(i, [4,5,6]);
          vn200_gyro(1, :) = vn200_data(i, [7,8,9]); 
          vn200_mag_compass(1, :) = vn200_data(i, [1,2,3]);
        else 
            vn200_accel(size(vn200_accel,1)+1, :) = vn200_data(i, [4,5,6]);
            vn200_gyro(size(vn200_gyro,1)+1, :) = vn200_data(i, [7,8,9]);
            vn200_mag_compass(size(vn200_mag_compass,1)+1, :) = vn200_data(i, [1,2,3]);
        end
    elseif vn200_txt_tag(i,1) == "$VNGPE"
        if size(vn200_gps_ECEF_xyz, 1) == 1 && vn200_gps_ECEF_xyz(1,1) == 0
           vn200_gps_ECEF_xyz(1, :) = vn200_data(i, [5,6,7]);
           vn200_gps_ECEF_vel(1, :) = vn200_data(i, [8,9,10]);
        else
            vn200_gps_ECEF_xyz(size(vn200_gps_ECEF_xyz,1)+1, :) = vn200_data(i, [5,6,7]);
            vn200_gps_ECEF_vel(size(vn200_gps_ECEF_vel,1)+1, :) = vn200_data(i, [8,9,10]);
        end
    end
end

%% Save and export files for GPS and IMU data
save('VN200_GPS_Data.mat','vn200_gps_ECEF_xyz','vn200_gps_ECEF_vel','vn200_gps_Fs');
save('VN200_IMU_Data.mat','vn200_accel','vn200_gyro','vn200_mag_compass','vn200_imu_Fs');


