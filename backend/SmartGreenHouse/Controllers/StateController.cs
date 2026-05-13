using Microsoft.AspNetCore.Mvc;
using SmartGreenHouse.Models;
namespace SmartGreenHouse.Controllers;

[ApiController]
[Route("state")]
public class StateController : ControllerBase
{
    private static GreenhouseState CurrentState = new();
    [HttpGet]
    public IActionResult GetState()
    {
        return Ok(CurrentState);
    }
    [HttpPost]
    public IActionResult UpdateState([FromBody] GreenhouseState state)
    {
        CurrentState = state;
        return Ok();
    }
}