using Microsoft.AspNetCore.Mvc;
using SmartGreenHouse.Models;

namespace SmartGreenHouse.Controllers;

[ApiController]
[Route("configuration")]
public class ConfigurationController : ControllerBase
{
    private static readonly List<PlantConfiguration> _configurations = [];
    private static PlantConfiguration? _currentConfiguration;

    public ConfigurationController()
    {
        if (!_configurations.Any())
        {
            _configurations.AddRange([
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Микрозелень",
                    Fan=new FanData(){ interval=35, run=1},
                    Hum=new HumData(){ interval=15, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=10, cooldown=1},
                    Soil = new SoilData(){ dry=300, wet=600, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Зеленый лук",
                     Fan=new FanData(){ interval=35, run=1},
                    Hum=new HumData(){ interval=25, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=500},
                    Pump=new PumpData(){ run=15, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Базилик ",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Мята",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Черри-томаты",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Зеленый лук",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Фиалка",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Герань",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                },
                new PlantConfiguration
                {
                    Id = Guid.NewGuid(),
                    Name = "Овес",
                     Fan=new FanData(){ interval=5, run=1},
                    Hum=new HumData(){ interval=5, run=1},
                    Light = new LightData(){ force_interval =5, force_duration=1, threshold=200},
                    Pump=new PumpData(){ run=5, cooldown=1},
                    Soil = new SoilData(){ dry=500, wet=300, ema=0.1}
                }
            ]);
        }
    }

    [HttpGet]
    public IActionResult GetConfigurations()
    {
        return Ok(_configurations);
    }

    [HttpPost]
    public IActionResult SaveConfiguration([FromBody] PlantConfiguration
    configuration)
    {
        var existing = _configurations.FirstOrDefault(x => x.Id == configuration.Id);

        if (existing == null)
        {
            _configurations.Add(configuration);
        }
        else
        {
            existing.Name = configuration.Name;
            existing.Hum = configuration.Hum;
            existing.Light = configuration.Light;
            existing.Pump = configuration.Pump;
            existing.Soil = configuration.Soil;
        }
        return Ok(configuration);
    }

    [HttpPost("current")]
    public IActionResult SetCurrent([FromBody] CurrentConfigurationRequest
    request)
    {
        _currentConfiguration = _configurations.FirstOrDefault(x => x.Id == request.ConfigurationId);

        if (_currentConfiguration == null)
            return NotFound();

        return Ok(_currentConfiguration);
    }
    [HttpGet("current")]
    public IActionResult GetCurrent()
    {
        if (_currentConfiguration == null)
            return NotFound();

        return Ok(_currentConfiguration);
    }
}
