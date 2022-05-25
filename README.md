# air_quality_sensor
Developed Arduino code for getting air quality data from Plantower PMS5003 sensor and outputting the AQI to an OLED SSD1351 screen.  

Air quality sensor, OLED screen, and Arduino are housed in a custom 3D printed box and powered by a 5V power supply.

The Plantower PMS5003 sensor takes measurements of the air and outputs the parts per milion (PPM) measurment for 2.5uM and 10uM particles.

Air quality data is typically not provided as PPM of 2.5uM and 10uM particles, but rather as AQI.  In order to provide the user with more relevant data, the PPM measurements must be converted to the U.S. Air Quality Index (AQI).

The Arduino code utilises a Circular buffer library to store PPM measurements for the last 10 min, 1 hr, and 1 day.  It then uses the buffers to calculate the AQI for the last 10 min, 1 hour, and 1 day.   

The Arduino code then drives the OLED screen and displays the AQI readings, as well as the last PPM measurement.  There is also a function that takes in the AQI value, then will display either a Green, Yellow, Orange, Red, Purple, or Maroon box around the air quality reading based on the colors defined by the U.S. AQI chart.

There is also a button on the side of the housing that allows the user to tell the display to switch between the AQI readings for 2.5uM particles and 10uM particles.
