
import Foundation;

struct StringIterator {
  var curr: String.UnicodeScalarView.Element?
  var iter: String.UnicodeScalarView.Iterator

  init(_ value: String) {
    iter = value.unicodeScalars.makeIterator()
    curr = iter.next()
  }
  
  func peek() -> String.UnicodeScalarView.Element? {
    return curr;
  }
  
  mutating func read() {
    curr = iter.next();
  }
  
  mutating func readString() -> String? {
    read();
    
    var result = "";
    while let char = peek() {
      switch char.value {
        case 0x22: // "
          read();
          return result;
          
        case 0x5c: // \
          read();
          guard let next = peek() else {
            return nil;
          }
          switch next.value {
            case 0x22: // "
              result.append("\"");
            case 0x5c: // \
              result.append("\\");
            case 0x2f: // /
              result.append("/");
            case 0x62: // b
              result.append("\u{08}");
            case 0x66: // f
              result.append("\u{0c}");
            case 0x6e: // n
              result.append("\u{0a}");
            case 0x72: // r
              result.append("\u{0d}");
            case 0x74: // t
              result.append("\u{09}");
            case 0x75: // u
              guard let unicode = readCharacter() else {
                return nil;
              }
              result.append(unicode);
            default:
              return nil;
          }
        
        
        default:
          result.append("\(char)");
      }
      read();
    }
    
    return nil;
  }
  
  private mutating func readHexValue() -> UInt16? {
    var counter = 0;
    var result: UInt16 = 0;
    while let char = peek() {
      switch char.value {
        case 0x30..<0x3a: // 0..<=9
          result = (result << 4)|UInt16(char.value - 0x30);
        case 0x61..<0x67: // a..<=f
          result = (result << 4)|UInt16(char.value - 0x61 + 10);
        case 0x41..<0x47: // A..<=F
          result = (result << 4)|UInt16(char.value - 0x41 + 10);
        default:
          return nil;
      }
      counter += 1;
      if counter == 4 {
        return result;
      }
      read();
    }
    return nil;
  }
  
  mutating func readCharacter() -> Character? {
    read();
    guard let value = readHexValue() else {
      return nil;
    }
    let scalar: Unicode.Scalar?;
    switch value {
      // high-surrogate pair
      case 0xd800..<0xdc00:
        read();
        let high = UInt32(value);
        // need a low-surrogate after a high-surrogate
        guard expect(literal: "\\u") else {
          return nil;
        }
        guard let low = readHexValue() else {
          return nil;
        }
        guard 0xdc00 <= low && low <= 0xdfff else {
          return nil;
        }
        let value = ((high - 0xd800) << 10) + UInt32(low - 0xdc00) + 0x10000;
        scalar = Unicode.Scalar(value);
      
      // low-surrogate pair, invalid if not after high-surrogate
      case 0xdc00..<0xe000:
        return nil;
        
      default:
        scalar = Unicode.Scalar(value);
    }
    guard let unicode = scalar else {
      return nil;
    }
    return Character(unicode);
  }

  mutating func expect(literal value: String) -> Bool {
    for char in value.unicodeScalars {
      guard let next = peek() else {
        return false;
      }
      
      if next != char {
        return false;
      }
      
      read();
    }
    return true
  }
  
  mutating func skipSpace() {
    while let element = curr {
      switch element.value {
        case 0x20,0x09,0x0a,0x0d:
          curr = iter.next();
        default:
          return;
      }
    }
  }
}

class JSONParser {
  var iter: StringIterator

  init(JSON data: String) {
    iter = StringIterator(data)
  }
  
  func parse() -> JSONValue? {
    guard let value = parseValue() else {
      return nil;
    }

    if let _ = iter.peek() {
      return nil;
    }
    
    return value;
  }
  
  private func parseValue() -> JSONValue? {
    iter.skipSpace();
    guard let next = iter.peek() else {
      return nil;
    }
  
    let result: JSONValue?;
    switch next.value {
      case 0x6e: // n
        result = parseNull();
      case 0x74: // t
        result = parseTrue();
      case 0x66: // f
        result = parseFalse();
      case 0x5b: // [
        result = parseArray();
      case 0x7b: // {
        result = parseObject();
      case 0x22: // "
        result = parseString();
      case 0x30..<0x3a, 0x2d: // 0-9, -
        result = parseNumber();
      default:
        return nil;
    }

    if let _ = result {
      iter.skipSpace();
    }

    return result;
  }

  private func parseNull() -> JSONValue? {
    guard iter.expect(literal: "null") else {
      return nil;
    }
    return JSONValue.Null;
  }

  private func parseTrue() -> JSONValue? {
    guard iter.expect(literal: "true") else {
      return nil;
    }
    return JSONValue.True;
  }

  private func parseFalse() -> JSONValue? {
    guard iter.expect(literal: "false") else {
      return nil;
    }
    return JSONValue.False;
  }

  private func parseArray() -> JSONValue? {
    iter.read();
    iter.skipSpace();

    // Is this an empty array?
    if let char = iter.peek() {
      switch char.value {
        case 0x5d: // ]
          iter.read();
          return JSONValue.Array([]);
        default:
          break;
      }
    }
    
    var result = [JSONValue]();
    while let _ = iter.peek() {
      guard let json = parseValue() else {
        return nil;
      }
      result.append(json);
      guard let next = iter.peek() else {
        return nil;
      }
      switch next.value {
        case 0x2c: // ,
          iter.read();
          iter.skipSpace();
        case 0x5d: // ]
          iter.read();
          return JSONValue.Array(result);
        default:
          return nil;
      }
    }
    return nil;
  }

  private func parseObject() -> JSONValue? {
    iter.read();
    iter.skipSpace();

    // Is this an empty object?
    if let char = iter.peek() {
      switch char.value {
        case 0x7d: // ]
          iter.read();
          return JSONValue.Object(Dictionary());
        default:
          break;
      }
    }

    var result = Dictionary<String, JSONValue>();
    while let char = iter.peek() {
      switch char.value {
        case 0x22: // "
          guard let key = iter.readString() else {
            return nil;
          }
          iter.skipSpace();
          guard let sep = iter.peek() else {
            return nil;
          }
          switch sep.value {
            case 0x3a: // :
              iter.read();
              iter.skipSpace();
            default:
              return nil;
          }
          guard let val = parseValue() else {
            return nil;
          }
          result[key] = val;
          guard let next = iter.peek() else {
            return nil;
          }
          switch next.value {
            case 0x2c: // ,
              iter.read();
              iter.skipSpace();
            case 0x7d: // }
              iter.read();
              return JSONValue.Object(result);
            default:
              return nil;
          }
          
        default:
          return nil;
      }
    }
    return nil;
  }

  private func parseString() -> JSONValue? {
    guard let string = iter.readString() else {
      return nil;
    }
    return JSONValue.String(string)
  }

  private func parseNumber() -> JSONValue? {
    var repr = "";
    
    switch iter.peek() {
      case .none:
        return nil;
      
      case let .some(char) where char.value == 0x2d:
        iter.read();
        repr.append(Character(char));
      
      default:
        break;
    }

    switch iter.peek() {
      case .none:
        return nil;
      
      case let .some(char) where char.value == 0x30:
        iter.read();
        repr.append(Character(char));
      
      case let .some(char) where 0x31 <= char.value && char.value <= 0x39:
        iter.read();
        repr.append(Character(char));
        while let char = iter.peek() {
          switch char.value {
            case 0x30..<0x3a: // 0..<=9
              iter.read();
              repr.append(Character(char));
              continue;
              
            default:
              break;
          }
          break;
        }

      default:
        return nil;
    }
    
    if let char = iter.peek() {
      switch char.value {
        case 0x2e: // .
          iter.read();
          repr.append(Character(char));
          switch iter.peek() {
            case let .some(char) where 0x30 <= char.value && char.value <= 0x39:
              iter.read();
              repr.append(Character(char));
              while let char = iter.peek() {
                switch char.value {
                  case 0x30..<0x3a: // 0..<=9
                    iter.read();
                    repr.append(Character(char));
                    continue;
                    
                  default:
                    break;
                }
                break;
              }
              
            default:
              return nil;
          }
          
          default:
            break;
      }
    }

    if let char = iter.peek() {
      switch char.value {
        case 0x45, 0x65: // E, e
          iter.read();
          repr.append(Character(char));
          guard let sign = iter.peek() else {
            return nil;
          }
          
          switch sign.value {
            case 0x2b, 0x2d: // +, -
              iter.read();
              repr.append(Character(sign));
              
            default:
              break;
          }

          guard let next = iter.peek() else {
            return nil;
          }
          
          switch next.value {
            case 0x30..<0x3a: // 0..<=9
              iter.read();
              repr.append(Character(next));
              while let char = iter.peek() {
                switch char.value {
                  case 0x30..<0x3a: // 0..<=9
                    iter.read();
                    repr.append(Character(char));
                    continue;
                    
                  default:
                    break;
                }
                break;
              }

            default:
              return nil;
          }

          default:
            break;
        }
    }

    guard let number = Double(repr) else {
      return nil;
    }
    
    return JSONValue.Number(number);
  }
}

enum JSONValue {
  case Null
  case True
  case False
  case Number(Double)
  case String(String)
  case Array([JSONValue])
  case Object(Dictionary<String, JSONValue>)
}

extension JSONValue : CustomStringConvertible {
  var description: String {
    switch self {
      case .Null:
        return "null"
      case .True:
        return "true"
      case .False:
        return "false"
      case let .Number(d):
        return "\(d)"
      case let .String(s):
        return "\(escaped: s)"
      case let .Array(arr):
        var output = "";
        output.append("[");
        for (index, element) in arr.enumerated() {
          if (index != 0) {
            output.append(", ");
          }
          output.append("\(element)")
        }
        output.append("]");
        return output
      case let .Object(obj):
        var output = "";
        output.append("{");
        for (index, element) in obj.enumerated() {
          if (index != 0) {
            output.append(", ");
          }
          output.append("\(escaped: element.key): \(element.value)")
        }
        output.append("}");
        return output
    }
  }
}

extension DefaultStringInterpolation {
  fileprivate mutating func appendInterpolation(escaped value: String) {
    appendLiteral("\"")
    for char in value.unicodeScalars {
      switch char.value {
        case 0x22: // "
          appendLiteral("\\\"")
        case 0x5c: // \
          appendLiteral("\\\\")
        case 0x08: // backspace
          appendLiteral("\\b")
        case 0x0c: // form feed
          appendLiteral("\\f")
        case 0x0a: // line feed
          appendLiteral("\\a")
        case 0x0d: // carriage return
          appendLiteral("\\r")
        case 0x09: // tab
          appendLiteral("\\t")
        case 0..<0x20: // control
          appendLiteral("\\u" + String(format: "%04x", char.value))
        default:
          appendLiteral(String(char))
      }
    }
    appendLiteral("\"")
  }
}

@objc
public class Sanitizer : NSObject {
  @objc
  public func sanitize(JSON data: String) -> String? {
    let parser = JSONParser(JSON: data);
    if let json = parser.parse() {
      return String(describing: json)
    } else {
      return nil
    }
  }
}
