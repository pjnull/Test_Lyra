using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet
{
	/// <summary>
	/// A class that can be serialized in from JSON and used in a TestNode to ignore specific warnings/errors if so desired
	/// </summary>
	public class IgnoredIssueList
	{
		public IgnoredIssueList()
		{

		}

		private IEnumerable<IgnoredTestIssues> IgnoredIssues { get; set; } = Enumerable.Empty<IgnoredTestIssues>();

		public bool LoadFromFile(string InPath)
		{
			try
			{
				IgnoredIssues = JsonConvert.DeserializeObject<IgnoredTestIssues[]>(File.ReadAllText(InPath));
			}
			catch (Exception Ex)
			{
				Log.Warning("Failed to load issue definition from {0}. {1}", InPath, Ex.Message);
				return false;
			}

			return true;
		}

		public bool IsEnsureIgnored(string InTestName, string InEnsureMsg)
		{
			IEnumerable<IgnoredTestIssues> TestDefinitions = IgnoredIssues.Where(I => I.TestName == "*" || I.TestName.Equals(InTestName, StringComparison.OrdinalIgnoreCase));
			return TestDefinitions.Any(D => D.IgnoredEnsures.Any(Ex => InEnsureMsg.IndexOf(Ex, StringComparison.OrdinalIgnoreCase) >= 0));

		}

		public bool IsWarningIgnored(string InTestName, string InWarning)
		{
			IEnumerable<IgnoredTestIssues> TestDefinitions = IgnoredIssues.Where(I => I.TestName == "*" || I.TestName.Equals(InTestName, StringComparison.OrdinalIgnoreCase));
			return TestDefinitions.Any(D => D.IgnoredWarnings.Any( Ex => InWarning.IndexOf(Ex, StringComparison.OrdinalIgnoreCase) >= 0));

		}

		public bool IsErrorIgnored(string InTestName, string InError)
		{
			IEnumerable<IgnoredTestIssues> TestDefinitions = IgnoredIssues.Where(I => I.TestName.Equals(InTestName, StringComparison.OrdinalIgnoreCase));
			return TestDefinitions.Any(D => D.IgnoredErrors.Any(Ex => InError.IndexOf(Ex, StringComparison.OrdinalIgnoreCase) >= 0));
		}
	}
}
