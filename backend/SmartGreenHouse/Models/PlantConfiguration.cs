namespace SmartGreenHouse.Models;

public class PlantConfiguration
{
    public Guid Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public LightData Light {  get; set; } 
    public SoilData Soil {  get; set; } 
    public PumpData Pump {  get; set; } 
    public FanData Fan {  get; set; } 
    public HumData Hum {  get; set; } 
}

public class LightData
{
    public int threshold { get; set; }
    public int force_interval { get; set; }
    public int force_duration { get; set; }
}

public class SoilData
{
    public int dry { get; set; }
    public int wet { get; set; }
    public double ema { get; set; }
}

public class PumpData
{
    public int run { get; set; }
    public int cooldown { get; set; }
}

public class FanData
{
    public int run { get; set; }
    public int interval { get; set; }
}

public class HumData
{
    public int run { get; set; }
    public int interval { get; set; }
}