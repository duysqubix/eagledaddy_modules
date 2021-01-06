
Prototype Module (0x001a):

Commands:

    RequestTime:            0x1d   // request time from Real world clock.
    RequestTempHumidity :   0x2b // request temperature
        Example:
            Master = [0x00, 0x1a, 0x2b]
            Slave = [0x00, 0x1a, 0x00, 0x04, 0x00, 0x1c]
    
    RequestDistance:        0x3c // request distance
    RequestMotorTime:       0x4a // request motor time


Example:
    Master -
        [0x00, 0x1a,  {cmd}, {data}]

    Slave -
        [0x00, 0x1a,  {data}]


Deer Feeder Module (0x002b)

    Master -
    [0x00, 0x2b]




Some metadata is stored on uC in case of power shut off - these values need to be read in order to 
activate some activity without human interaction..

Prototype Module:
    * Relay Timer ( times for motor to turn on stored, in seconds)
    * Relay Schedule (allows up to 10 times a day, that will causes relay to start for _RELAY_TIMER_ time) 
