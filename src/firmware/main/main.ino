int
    Zoom_Follow_pin = A0,
    Zoom_Follow_val,
    Focus_Follow_pin = A1,
    Focus_Follow_val;

void setup() {
    pinMode(Zoom_Follow_pin, INPUT);
    pinMode(Focus_Follow_pin, INPUT);
    Serial.begin(115200);

    /*
     * Serial1 is for connecting RS-422 interface
     * 
     * Bit Conf iguration
     * a. 1 start bit ( space )
     * b. 8 data bits
     * c. 1 par ity bit (odd)
     * d. 1 stop bit (mar k)
     * e. Byte time = .286 mse c .
     */
    Serial1.begin(38400, SERIAL_8O1);
}

#define SLEEP_BEFORE_TRY_READ 4

void loop()
{
    analogRead(Focus_Follow_pin);
    delay(SLEEP_BEFORE_TRY_READ);
    Focus_Follow_val = analogRead(Focus_Follow_pin);

    analogRead(Zoom_Follow_pin);
    delay(SLEEP_BEFORE_TRY_READ);
    Zoom_Follow_val = analogRead(Zoom_Follow_pin);

    Serial1.flush();
    Serial.flush();
    
    Serial.print(Focus_Follow_val, DEC);
    Serial1.print(Focus_Follow_val, DEC);
    Serial.print("/");
    Serial1.print("/");

    Serial.println(Zoom_Follow_val, DEC);
    Serial1.println(Zoom_Follow_val, DEC);
//    Serial.println(Zoom_Follow_val / 1024.0 * 4.6, 4);
//    delay(10);
}
