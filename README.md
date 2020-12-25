<a href="#">
    <img width="192px" height="192px" src="doc/icon.png" align="right" />
</a>

# Serial Studio

[![Build Status](https://travis-ci.org/Serial-Studio/Serial-Studio.svg?branch=master)](https://travis-ci.org/Serial-Studio/Serial-Studio)

Serial Studio is a multi-platform, multi-purpose serial data visualization program. The goal of this project is to allow embedded developers & makers to easily visualize, present & analyze the data generated by their projects and devices, without the need of writing specialized computer software for each project.

The need for this project arose during the development of the Ground Station Software for several CanSat-based competitions in which I participate. It's simply not sustainable to develop and maintain different GSS programs for each competition & project. The smart solution is to have one common Ground Station software and let each CanSat define how the data is presented to the end user by using an extensible communication protocol.

Furthermore, this approach can be extended to almost any type of project that involves some kind of data acquisition & measurement.

![Screenshot](doc/screenshot.png)

## Communication protocol (OBLIGATORY READ)

The communication protocol is implemented through a JSON document with the following structure:

- **`t`**: project title (*string*, obligatory)
- **`g`**: groups (*array*)
  - **`t`**: group title (*string*, obligatory)
  - **`w`**: Widget type (*string*; optional - can be:)
    - `map`: create a widget showing a location on a map
    - `bar`: vertical progress bar (with `max` & `min` values)
    - `gauge`: semi-circular gauge (with `max` & `min` values)
    - `gyro`: gyroscope indicator (with `x`, `y` & `z` values)
    - `accelerometer`: accelerometer indicator (with `x`, `y`, & `z` values)
    - `tank`: vertical tank indicator (with `max` & `min` values)
  - **`d`**: group datasets (*array*)
    - **`t`**:  dataset title (*string*, optional)
    - **`v`**:  dataset value (*variant*, obligatory)
    - **`u`**:  dataset unit (*string*, optional)
    - **`g`**:  dataset graph (*boolean*, optional)
    - **`w`**:  widget type (*string*, depends group widget type, posible values are:)
        - For `gyro` & `accelerometer` widgets: 
            - `x`: value for X axis
            - `y`: value for Y axis
            - `z`: value for Z axis
        - For `map` widget: 
            - `lat`: latitude
            - `lon`: longitude
        - For `bar`, `tank` & `gauge` widgets:
            - `max`: maximum value
            - `min`: minimum value
            - `value`: current value
    
This information is processed by Serial Studio, which builds the user interface according to the information contained in each frame. This information is also used to generate a CSV file with all the readings received from the serial device, the CSV file can be used for analysis and data-processing within MATLAB.

**NOTE:** widget types can be repeated across different groups without any problem.

### Communication modes

Serial Studio can process incoming serial information in two ways:

1. The serial device sends a full JSON data frame periodically (**auto mode**).
2. User specifies the JSON structure in a file, and the serial device only sends data in a comma separated manner (**manual mode**).

The manual mode is useful if you don't want to use a JSON library in your microcontroller program, or if you need to send large ammounts of information. An example of a JSON *map* file is:

```json
{
   "t":"%1",
   "g":[
      {
         "t":"Mission Status",
         "d":[
            {
               "t":"Runtime",
               "v":"%2",
               "u":"ms"
            },
            {
               "t":"Packet count",
               "v":"%3"
            },
            {
               "t":"Battery voltage",
               "v":"%4",
               "g":true,
               "u":"V"
            }
         ]
      },
      {
         "t":"Sensor Readings",
         "d":[
            {
               "t":"Temperature",
               "v":"%5",
               "g":true,
               "u":"°C"
            },
            {
               "t":"Altitude",
               "v":"%6",
               "u":"m"
            },
            {
               "t":"Pressure",
               "v":"%7",
               "u":"KPa",
               "g":true
            },
            {
               "t":"External Temperature",
               "v":"%8",
               "g":true,
               "u":"°C"
            },
            {
               "t":"Humidity",
               "v":"%9",
               "g":true,
               "u":"°C"
            }
         ]
      },
      {
         "t":"GPS",
         "w":"map",
         "d":[
            {
               "t":"GPS Time",
               "v":"%10"
            },
            {
               "t":"Longitude",
               "v":"%11",
               "u":"°N",
               "w":"lon"
            },
            {
               "t":"Latitude",
               "v":"%12",
               "u":"°E",
               "w":"lat"
            },
            {
               "t":"Altitude",
               "v":"%13",
               "u":"m"
            },
            {
               "t":"No. Sats",
               "v":"%14"
            }
         ]
      },
      {
         "t":"Accelerometer",
         "w":"accelerometer",
         "d":[
            {
               "t":"X",
               "v":"%15",
               "u":"m/s^2",
               "g":true,
               "w":"x"
            },
            {
               "t":"Y",
               "v":"%16",
               "u":"m/s^2",
               "g":true,
               "w":"y"
            },
            {
               "t":"Z",
               "v":"%17",
               "u":"m/s^2",
               "g":true,
               "w":"z"
            }
         ]
      },
      {
         "t":"Gyroscope",
         "w":"gyro",
         "d":[
            {
               "t":"X",
               "v":"%18",
               "u":"rad/s",
               "g":true,
               "w":"x"
            },
            {
               "t":"Y",
               "v":"%19",
               "u":"rad/s",
               "g":true,
               "w":"y"
            },
            {
               "t":"Z",
               "v":"%20",
               "u":"rad/s",
               "g":true,
               "w":"z"
            }
         ]
      }
   ]
}
```

As you can guess, *Serial Studio* will replace the `%1`, `%2`, `%3`, `...`, `%20` values with the values at the corresponding index in a comma-separated data frame. The corresponding data format sent by the microcontroller for the given JSON map is:

`/*KAANSATQRO,%s,%s,%s,%s,%s,%s,%,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s*/`

### Frame start/end sequences

To process all data frames, Serial Studio needs to have a reliable way to know when a frame starts and when a frame ends. The solution that I came with is to have a specific start/end sequence, which corresponds to:

- `/*` Frame start sequence
- `*/` Frame end sequence

The start/end sequences apply both to the **auto** & **manual** communication modes.

### Example

Supose that you are receiving the following data from a microcontroller:

`/*KAANSATQRO,2051,2,5,26,10,101.26,27,32,1001,21.1619,86.8515,10,4,1.23,9.81,0.23,0,0,0*/`

Serial Studio is configured to interpret incoming data using the JSON map file presented above. The data will be separated as:

| Index                        |  0           |  1     |  2   |  3   |  4    |  5    |  6        |  7    |  8    |  9      |  10        |  11        |  12   |  13   |  14     |  15     |  16     |  17   |  18   |  19   |
|------------------------------|--------------|--------|------|------|-------|-------|-----------|-------|-------|---------|------------|------------|-------|-------|---------|---------|---------|-------|-------|-------|
| JSON map match       | `%1`         | `%2`   | `%3` | `%4` | `%5`  | `%6`  | `%7`      | `%8`  | `%9`  | `%10`   | `%11`      | `%12`      | `%13` | `%14` | `%15`   | `%16`   | `%17`   | `%18` | `%19` | `%20` |
| Replaced with         | `KAANSATQRO` | `2051` | `2`  | `5`  | `26`  | `10`  | `101.26`  | `27`  | `32`  | `1001`  | `21.1619`  | `86.8515`  | `10`  | `4`   | `1.23`  | `9.81`  | `0.23`  | `0`   | `0`   | `0`   | 

All incoming data frames will be automatically registered in a CSV file, which can be used for later analysis.

## Build instructions

##### Requirements

The only requirement to compile the application is to have [Qt](http://www.qt.io/download-open-source/) installed in your system. The desktop application will compile with Qt 5.15 or greater.

### Cloning this repository

This repository makes use of [`git submodule`](https://git-scm.com/docs/git-submodule). In order to clone it, you have two options:

One-liner:

    git clone --recursive https://github.com/Serial-Studio/Serial-Studio

Normal procedure:

    git clone https://github.com/Serial-Studio/Serial-Studio
    cd Serial-Studio
    git submodule init
    git submodule update
    
###### Compiling the application

Once you have Qt installed, open *Serial-Studio.pro* in Qt Creator and click the "Run" button.

Alternatively, you can also use the following commands:

	qmake
	make -j4

## Licence

This project is released under the MIT license, for more information, check the [LICENSE](LICENSE.md) file.



