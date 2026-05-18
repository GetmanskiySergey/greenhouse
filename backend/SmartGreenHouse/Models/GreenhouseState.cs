namespace SmartGreenHouse.Models;
public class GreenhouseState
{
    public float Temperature { get; set; }
    public float Water { get; set; }
    public float Soil { get; set; }
    public float Humidity { get; set; }
    public float Lux { get; set; }
    public bool Pump { get; set; }
    public bool Fan { get; set; }
    public bool Hum { get; set; }
    public bool Light { get; set; }
}
