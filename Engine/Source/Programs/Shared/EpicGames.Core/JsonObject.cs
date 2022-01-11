// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace EpicGames.Core
{
	/// <summary>
	/// Exception thrown for errors parsing JSON files
	/// </summary>
	public class JsonParseException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Optional arguments</param>
		public JsonParseException(string Format, params object[] Args)
			: base(string.Format(Format, Args))
		{
		}
	}

	/// <summary>
	/// Stores a JSON object in memory
	/// </summary>
	public class JsonObject
	{
		readonly Dictionary<string, object?> RawObject;

		/// <summary>
		/// Construct a JSON object from the raw string -> object dictionary
		/// </summary>
		/// <param name="InRawObject">Raw object parsed from disk</param>
		public JsonObject(Dictionary<string, object?> InRawObject)
		{
			RawObject = new Dictionary<string, object?>(InRawObject, StringComparer.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Element"></param>
		public JsonObject(JsonElement Element)
		{
			RawObject = new Dictionary<string, object?>(StringComparer.InvariantCultureIgnoreCase);
			foreach (JsonProperty Property in Element.EnumerateObject())
			{
				RawObject[Property.Name] = ParseElement(Property.Value);
			}
		}

		/// <summary>
		/// Parse an individual element
		/// </summary>
		/// <param name="Element"></param>
		/// <returns></returns>
		public static object? ParseElement(JsonElement Element)
		{
			switch(Element.ValueKind)
			{
				case JsonValueKind.Array:
					return Element.EnumerateArray().Select(x => ParseElement(x)).ToArray();
				case JsonValueKind.Number:
					return Element.GetDouble();
				case JsonValueKind.Object:
					return Element.EnumerateObject().ToDictionary(x => x.Name, x => ParseElement(x.Value));
				case JsonValueKind.String:
					return Element.GetString();
				case JsonValueKind.False:
					return false;
				case JsonValueKind.True:
					return true;
				case JsonValueKind.Null:
					return null;
				default:
					throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Read a JSON file from disk and construct a JsonObject from it
		/// </summary>
		/// <param name="File">File to read from</param>
		/// <returns>New JsonObject instance</returns>
		public static JsonObject Read(FileReference File)
		{
			string Text = FileReference.ReadAllText(File);
			try
			{
				return Parse(Text);
			}
			catch(Exception Ex)
			{
				throw new JsonParseException("Unable to parse {0}: {1}", File, Ex.Message);
			}
		}

		/// <summary>
		/// Tries to read a JSON file from disk
		/// </summary>
		/// <param name="FileName">File to read from</param>
		/// <param name="Result">On success, receives the parsed object</param>
		/// <returns>True if the file was read, false otherwise</returns>
		public static bool TryRead(FileReference FileName, [NotNullWhen(true)] out JsonObject? Result)
		{
			if (!FileReference.Exists(FileName))
			{
				Result = null;
				return false;
			}

			string Text = FileReference.ReadAllText(FileName);
			return TryParse(Text, out Result);
		}

		/// <summary>
		/// Parse a JsonObject from the given raw text string
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <returns>New JsonObject instance</returns>
		public static JsonObject Parse(string Text)
		{
			JsonDocument Document = JsonDocument.Parse(Text, new JsonDocumentOptions { AllowTrailingCommas = true });
			return new JsonObject(Document.RootElement);
		}

		/// <summary>
		/// Try to parse a JsonObject from the given raw text string
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Result">On success, receives the new JsonObject</param>
		/// <returns>True if the object was parsed</returns>
		public static bool TryParse(string Text, [NotNullWhen(true)] out JsonObject? Result)
		{
			try
			{
				Result = Parse(Text);
				return true;
			}
			catch (Exception)
			{
				Result = null;
				return false;
			}
		}

		/// <summary>
		/// List of key names in this object
		/// </summary>
		public IEnumerable<string> KeyNames
		{
			get { return RawObject.Keys; }
		}

		/// <summary>
		/// Gets a string field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public string GetStringField(string FieldName)
		{
			if (!TryGetStringField(FieldName, out string? StringValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValue;
		}

		/// <summary>
		/// Tries to read a string field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetStringField(string FieldName, [NotNullWhen(true)] out string? Result)
		{
			if (RawObject.TryGetValue(FieldName, out object? RawValue) && (RawValue is string))
			{
				Result = (string)RawValue;
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets a string array field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public string[] GetStringArrayField(string FieldName)
		{
			if (!TryGetStringArrayField(FieldName, out string[]? StringValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValues;
		}

		/// <summary>
		/// Tries to read a string array field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetStringArrayField(string FieldName, [NotNullWhen(true)] out string[]? Result)
		{
			if (RawObject.TryGetValue(FieldName, out object? RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is string))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => (string)x).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets a boolean field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public bool GetBoolField(string FieldName)
		{
			if (!TryGetBoolField(FieldName, out bool BoolValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return BoolValue;
		}

		/// <summary>
		/// Tries to read a bool field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetBoolField(string FieldName, out bool Result)
		{
			if (RawObject.TryGetValue(FieldName, out object? RawValue) && (RawValue is bool))
			{
				Result = (bool)RawValue;
				return true;
			}
			else
			{
				Result = false;
				return false;
			}
		}

		/// <summary>
		/// Gets an integer field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public int GetIntegerField(string FieldName)
		{
			if (!TryGetIntegerField(FieldName, out int IntegerValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return IntegerValue;
		}

		/// <summary>
		/// Tries to read an integer field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetIntegerField(string FieldName, out int Result)
		{
			if (!RawObject.TryGetValue(FieldName, out object? RawValue) || !int.TryParse(RawValue?.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Tries to read an unsigned integer field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetUnsignedIntegerField(string FieldName, out uint Result)
		{
			if (!RawObject.TryGetValue(FieldName, out object? RawValue) || !uint.TryParse(RawValue?.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets a double field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public double GetDoubleField(string FieldName)
		{
			if (!TryGetDoubleField(FieldName, out double DoubleValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return DoubleValue;
		}

		/// <summary>
		/// Tries to read a double field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetDoubleField(string FieldName, out double Result)
		{
			if (!RawObject.TryGetValue(FieldName, out object? RawValue) || !double.TryParse(RawValue?.ToString(), out Result))
			{
				Result = 0.0;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets an enum field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public T GetEnumField<T>(string FieldName) where T : struct
		{
			if (!TryGetEnumField(FieldName, out T EnumValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return EnumValue;
		}

		/// <summary>
		/// Tries to read an enum field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetEnumField<T>(string FieldName, out T Result) where T : struct
		{
			if (!TryGetStringField(FieldName, out string? StringValue) || !Enum.TryParse<T>(StringValue, true, out Result))
			{
				Result = default;
				return false;
			}
			return true;
		}

		/// <summary>
		/// Tries to read an enum array field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetEnumArrayField<T>(string FieldName, [NotNullWhen(true)] out T[]? Result) where T : struct
		{
			if (!TryGetStringArrayField(FieldName, out string[]? StringValues))
			{
				Result = null;
				return false;
			}

			T[] EnumValues = new T[StringValues.Length];
			for (int Idx = 0; Idx < StringValues.Length; Idx++)
			{
				if (!Enum.TryParse<T>(StringValues[Idx], true, out EnumValues[Idx]))
				{
					Result = null;
					return false;
				}
			}

			Result = EnumValues;
			return true;
		}

		/// <summary>
		/// Gets an object field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public JsonObject GetObjectField(string FieldName)
		{
			if (!TryGetObjectField(FieldName, out JsonObject? Result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return Result;
		}

		/// <summary>
		/// Tries to read an object field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetObjectField(string FieldName, [NotNullWhen(true)] out JsonObject? Result)
		{
			if (RawObject.TryGetValue(FieldName, out object? RawValue) && (RawValue is Dictionary<string, object?>))
			{
				Result = new JsonObject((Dictionary<string, object?>)RawValue);
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		/// <summary>
		/// Gets an object array field by the given name from the object, throwing an exception if it is not there or cannot be parsed.
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <returns>The field value</returns>
		public JsonObject[] GetObjectArrayField(string FieldName)
		{
			if (!TryGetObjectArrayField(FieldName, out JsonObject[]? Result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return Result;
		}

		/// <summary>
		/// Tries to read an object array field by the given name from the object
		/// </summary>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public bool TryGetObjectArrayField(string FieldName, [NotNullWhen(true)] out JsonObject[]? Result)
		{
			if (RawObject.TryGetValue(FieldName, out object? RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is Dictionary<string, object>))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => new JsonObject((Dictionary<string, object?>)x)).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}
	}
}
