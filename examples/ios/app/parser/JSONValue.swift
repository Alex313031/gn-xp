
enum JSONValue {
  case Null
  case Number(Float)
  case String(String)
  case List([JSONValue])
  case Dict(Dictionary<String, JSONValue>)
}
