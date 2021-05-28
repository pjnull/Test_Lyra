

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using Tools.DotNETCommon;

namespace Gauntlet
{
	public static class Horde
	{

		static public bool IsHordeJob
		{
			get
			{
				return !string.IsNullOrEmpty(JobId);
			}
		}

		static public string JobId
		{
			get
			{
				return Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			}
		}

		static public string StepId
		{
			get
			{
				return Environment.GetEnvironmentVariable("UE_HORDE_STEPID");
			}
		}

		public static void GenerateSummary()
		{
			try
			{
				if (!IsHordeJob)
				{
					return;
				}

				string LogFolder = CommandUtils.CmdEnv.LogFolder;

				string MarkdownFilename = "GauntletStepDetails.md";

				string Markdown = $"Gauntlet Artifacts: [{Globals.LogDir})](file://{Globals.LogDir}";

				File.WriteAllText(Path.Combine(LogFolder, MarkdownFilename), Markdown);

				using (JsonWriter Writer = new JsonWriter(new FileReference(Path.Combine(LogFolder, "GauntletStepDetails.report.json"))))
				{
					Writer.WriteObjectStart();

					Writer.WriteValue("scope", "Step");
					Writer.WriteValue("name", "Gauntlet Step Details");
					Writer.WriteValue("placement", "Summary");
					Writer.WriteValue("fileName", MarkdownFilename);

					Writer.WriteObjectEnd();
				}
			}
			catch (Exception Ex)
			{
				Log.Info("Exception while generating Horde summary\n{0}\n", Ex.Message);
			}
		}

	}
}
