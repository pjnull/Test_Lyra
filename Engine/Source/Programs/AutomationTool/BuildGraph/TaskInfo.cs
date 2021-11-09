// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Text;
using System.Xml;

namespace AutomationTool
{
	/// <summary>
	/// A task invocation
	/// </summary>
	public class TaskInfo
	{
		/// <summary>
		/// Line number in a source file that this task was declared. Optional; used for log messages.
		/// </summary>
		public Tuple<string, int> SourceLocation
		{
			get;
			set;
		}

		/// <summary>
		/// Name of the task
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Arguments for the task
		/// </summary>
		public Dictionary<string, string> Arguments { get; } = new Dictionary<string, string>();

		/// <summary>
		/// Constructor
		/// </summary>
		public TaskInfo(Tuple<string, int> SourceLocation, string Name)
		{
			this.SourceLocation = SourceLocation;
			this.Name = Name;
		}

		/// <summary>
		/// Write to an xml file
		/// </summary>
		/// <param name="Writer"></param>
		public void Write(XmlWriter Writer)
		{
			Writer.WriteStartElement(Name);
			foreach (KeyValuePair<string, string> Argument in Arguments)
			{
				Writer.WriteAttributeString(Argument.Key, Argument.Value);
			}
			Writer.WriteEndElement();
		}
	}
}
