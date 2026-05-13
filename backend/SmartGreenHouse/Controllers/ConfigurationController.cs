using Microsoft.AspNetCore.Mvc;
using SmartGreenHouse.Models;
using System;

namespace SmartGreenHouse.Controllers;

[ApiController]
[Route("configuration")]
public class ConfigurationController : ControllerBase
{
    private static List<PlantConfiguration> Configurations = new();
    private static PlantConfiguration? CurrentConfiguration;

    public ConfigurationController()
    {
        if (!Configurations.Any())
        {
            Configurations.Add(new PlantConfiguration
            {
                Id = Guid.NewGuid(),
                Name = "Микрозелень",
                SoilHumidity = 60,
                LightLevel = 80,
                WateringInterval = 24,
                VentilationInterval = 12
            });

            Configurations.Add(new PlantConfiguration
            {
                Id = Guid.NewGuid(),
                Name = "Овес",
                SoilHumidity = 60,
                LightLevel = 80,
                WateringInterval = 24,
                VentilationInterval = 12
            });
        }
    }

    [HttpGet]
    public IActionResult GetConfigurations()
    {
        return Ok(Configurations);
    }

    [HttpPost]
    public IActionResult SaveConfiguration([FromBody] PlantConfiguration
    configuration)
    {
        var existing = Configurations.FirstOrDefault(x => x.Id ==
        configuration.Id);
        if (existing == null)
        {
            configuration.Id = Configurations.Count + 1;
            Configurations.Add(configuration);
        }
        else
        {
            existing.Name = configuration.Name;
            existing.SoilHumidity = configuration.SoilHumidity;
            existing.LightLevel = configuration.LightLevel;
            existing.WateringInterval = configuration.WateringInterval;
            existing.VentilationInterval = configuration.VentilationInterval;
        }
        return Ok(configuration);
    }
    [HttpPost("current")]
    public IActionResult SetCurrent([FromBody] CurrentConfigurationRequest
    request)
    {
        CurrentConfiguration = Configurations
        .FirstOrDefault(x => x.Id == request.ConfigurationId);
        if (CurrentConfiguration == null)
            return NotFound();
        return Ok(CurrentConfiguration);
    }
    [HttpGet("current")]
    public IActionResult GetCurrent()
    {
        if (CurrentConfiguration == null)
            return NotFound();
        return Ok(CurrentConfiguration);
    }
}
