namespace SmartGreenHouse.Models;

public class PlantConfiguration
{
    public Guid Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public int SoilHumidity { get; set; }
    public int LightLevel { get; set; }
    public int WateringInterval { get; set; }
    public int VentilationInterval { get; set; }
}